#include "wifi_provision_web.h"
#include "nvs_manager.h"
#include "cJSON.h"
#include <string.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_http_server.h>
#include "wifi_provision_web_form.h"

#define WIFI_CONNECTED_EVENT BIT0
#define WIFI_FAIL_EVENT BIT1

static const char *TAG = "PROV_WEB";
static EventGroupHandle_t wifi_event_group = NULL;
static httpd_handle_t server = NULL;
static bool running = false;
static wifi_prov_web_config_t prov_cfg = {0};
static void (*user_callback)(void) = NULL;

void generate_ecokey_ssid(char *ssid, size_t len)
{
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_AP, mac));
    snprintf(ssid, len, "EcoKey_%02X%02X%02X", mac[3], mac[4], mac[5]);
}
// --------- Funciones internas y Handlers ---------
static void delayed_restart_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(7000));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // solo STA
    vTaskDelay(pdMS_TO_TICKS(2000));
    if (user_callback)
        user_callback();
    esp_restart(); //ACTIVAR PARA REINICIAR EL DISPOSITIVO 
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_form, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t custom_data_post_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"No data\"}");
        return ESP_FAIL;
    }
    buf[ret] = 0;
    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return ESP_FAIL;
    }
    cJSON *j_mac = cJSON_GetObjectItem(root, "mac_objetivo");
    cJSON *j_ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *j_pass = cJSON_GetObjectItem(root, "password");
    cJSON *j_temp = cJSON_GetObjectItem(root, "temporizador");

    if (!cJSON_IsString(j_mac) || !j_mac->valuestring ||
        !cJSON_IsString(j_ssid) || !j_ssid->valuestring ||
        !cJSON_IsString(j_pass) || !j_pass->valuestring ||
        !cJSON_IsString(j_temp) || !j_temp->valuestring)
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"Faltan o son inválidos los campos obligatorios\"}");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // Guardar datos en NVS vía nvs_manager
    nvs_manager_set_string("mac_objetivo", j_mac->valuestring);
    nvs_manager_set_string("ssid", j_ssid->valuestring);
    nvs_manager_set_string("password", j_pass->valuestring);
    nvs_manager_set_string("temporizador", j_temp->valuestring);

    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid, j_ssid->valuestring, sizeof(wifi_cfg.sta.ssid));
    strncpy((char *)wifi_cfg.sta.password, j_pass->valuestring, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;
    wifi_cfg.sta.scan_method = WIFI_FAST_SCAN;
    wifi_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    esp_wifi_disconnect();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());

    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group, WIFI_CONNECTED_EVENT | WIFI_FAIL_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(15000));
    httpd_resp_set_type(req, "application/json");
    if (bits & WIFI_CONNECTED_EVENT)
    {
        httpd_resp_sendstr(req, "{\"success\":true}");
        xTaskCreate(delayed_restart_task, "delayed_restart", 4096, NULL, 5, NULL);
    }
    else
    {
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"No se pudo conectar a la WiFi. Verifica la contraseña y prueba de nuevo.\"}");
    }
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t scan_wifi_get_handler(httpd_req_t *req)
{
    uint16_t ap_num = 0;
    wifi_ap_record_t ap_records[20];
    wifi_scan_config_t scan_conf = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true};
    esp_wifi_scan_start(&scan_conf, true);
    esp_wifi_scan_get_ap_num(&ap_num);
    if (ap_num > 20)
        ap_num = 20;
    esp_wifi_scan_get_ap_records(&ap_num, ap_records);

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < ap_num; i++)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", (const char *)ap_records[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", ap_records[i].rssi);
        cJSON_AddNumberToObject(item, "authmode", ap_records[i].authmode);
        cJSON_AddItemToArray(root, item);
    }
    char *resp = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(root);
    free(resp);
    return ESP_OK;
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Desconectado de WiFi. Provisioning NO reintenta.");
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_EVENT);
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "Cliente conectado al SoftAP");
            break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Conectado con IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    }
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t s = NULL;
    if (httpd_start(&s, &config) == ESP_OK)
    {
        httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL};
        httpd_register_uri_handler(s, &root);

        httpd_uri_t custom_data = {.uri = "/custom-data", .method = HTTP_POST, .handler = custom_data_post_handler, .user_ctx = NULL};
        httpd_register_uri_handler(s, &custom_data);

        httpd_uri_t scan_wifi = {.uri = "/scan", .method = HTTP_GET, .handler = scan_wifi_get_handler, .user_ctx = NULL};
        httpd_register_uri_handler(s, &scan_wifi);
    }
    return s;
}

static void stop_webserver(httpd_handle_t s)
{
    if (s)
        httpd_stop(s);
}

// -------------- API Pública ---------------

esp_err_t wifi_provision_web_start(const wifi_prov_web_config_t *config)
{
    if (running)
        return ESP_ERR_INVALID_STATE;
    if (!config)
        return ESP_ERR_INVALID_ARG;
    prov_cfg = *config;
    user_callback = config->on_provisioned;

    // Inicializar los componentes de red necesarios
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Verifica si ya existe un event loop antes de crearlo
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        // Solo reporta error si es diferente a ESP_ERR_INVALID_STATE
        // (ESP_ERR_INVALID_STATE significa que ya existe, lo cual es aceptable)
        ESP_LOGE(TAG, "Error creando event loop: %s", esp_err_to_name(err));
        return err;
    }

    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // Verificar si ya existe el netif para STA y crear uno si no existe
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif == NULL)
    {
        esp_netif_create_default_wifi_sta();
    }

    // Verificar si ya existe el netif para AP y crear uno si no existe
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif == NULL)
    {
        esp_netif_create_default_wifi_ap();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    char ssid[32];
    generate_ecokey_ssid(ssid, sizeof(ssid));

    wifi_config_t ap_cfg = {0};
    snprintf((char *)ap_cfg.ap.ssid, sizeof(ap_cfg.ap.ssid), "%s", ssid);
    snprintf((char *)ap_cfg.ap.password, sizeof(ap_cfg.ap.password), "%s", prov_cfg.ap_pass ? prov_cfg.ap_pass : "ecokey123");
    ap_cfg.ap.ssid_len = strlen((char *)ap_cfg.ap.ssid);
    ap_cfg.ap.channel = prov_cfg.ap_channel > 0 ? prov_cfg.ap_channel : 1;
    ap_cfg.ap.max_connection = prov_cfg.ap_max_conn > 0 ? prov_cfg.ap_max_conn : 4;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    server = start_webserver();
    running = true;
    ESP_LOGI(TAG, "SoftAP iniciado. SSID: %s, password: %s", ap_cfg.ap.ssid, ap_cfg.ap.password);
    return ESP_OK;
}

void wifi_provision_web_stop(void)
{
    if (!running)
        return;
    stop_webserver(server);
    server = NULL;
    running = false;
}

bool wifi_provision_web_is_running(void)
{
    return running;
}
#include "wifi_ap.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include "lwip/ip4_addr.h"

#define WIFI_TIMEOUT_MS 30000
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "WiFiManager";
static esp_netif_t *sta_netif = NULL;
static esp_netif_t *ap_netif = NULL;
static char device_id_str[18] = {0};
static EventGroupHandle_t wifi_event_group;

// Manejador de eventos WiFi
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP iniciado correctamente");
                break;
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "Cliente conectado - MAC: "MACSTR", AID: %d", 
                         MAC2STR(event->mac), event->aid);
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "Cliente desconectado - MAC: "MACSTR", AID: %d, Reason: %d", 
                         MAC2STR(event->mac), event->aid, event->reason);
                break;
            }
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Conectado a la red WiFi");
                break;
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGW(TAG, "Desconexión WiFi, razón: %d", event->reason);
                if (event->reason == WIFI_REASON_NO_AP_FOUND ||
                    event->reason == WIFI_REASON_AUTH_FAIL ||
                    event->reason == WIFI_REASON_ASSOC_FAIL) {
                    xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
                } else {
                    // Reintento de conexión para otros errores
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    esp_wifi_connect();
                }
                break;
            }
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

void WiFiManager_connect(const char *ssid, const char *password) {
    ESP_LOGI(TAG, "Conectando a red WiFi: %s", ssid);

    wifi_event_group = xEventGroupCreate();
    
    esp_netif_init();
    esp_event_loop_create_default();
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Registrar manejadores de eventos
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg = (wifi_pmf_config_t){ .capable = true, .required = false };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Esperar a que se conecte o falle
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          WIFI_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "✅ Conectado a %s", ssid);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGW(TAG, "❌ Fallo al conectar con %s", ssid);
    } else {
        ESP_LOGW(TAG, "❌ Timeout al conectar con %s", ssid);
    }

    // Limpiar manejadores de eventos
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));
    vEventGroupDelete(wifi_event_group);
}

void WiFiManager_initAP(const char *ssid, const char *password) {
    ESP_LOGI(TAG, "Iniciando AP: %s", ssid);

    esp_netif_init();
    esp_event_loop_create_default();
    ap_netif = esp_netif_create_default_wifi_ap();

    // Configurar IP estática para el AP
    esp_netif_ip_info_t ip_info = {0};
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ip_info);
    esp_netif_dhcps_start(ap_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Registrar manejador de eventos WiFi
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(ssid);
    
    if (password && strlen(password) >= 8) {
        strncpy((char *)wifi_config.ap.password, password, sizeof(wifi_config.ap.password));
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.channel = 1;
    wifi_config.ap.ssid_hidden = 0;
    wifi_config.ap.beacon_interval = 100;
    wifi_config.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    
    // Deshabilitar el ahorro de energía para mejorar la estabilidad
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    
    // Configurar potencia de transmisión al máximo
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(84)); // 84 = 20dBm, el máximo
    
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info_status;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info_status);
    ESP_LOGI(TAG, "AP iniciado. IP: " IPSTR, IP2STR(&ip_info_status.ip));
}

const char *WiFiManager_getDeviceID(void) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(device_id_str, sizeof(device_id_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return device_id_str;
}

// Función para detener WiFi
void WiFiManager_stop(void) {
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    
    if (sta_netif) {
        esp_netif_destroy(sta_netif);
        sta_netif = NULL;
    }
    
    if (ap_netif) {
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
    }
    
    ESP_LOGI(TAG, "WiFi detenido correctamente");
}
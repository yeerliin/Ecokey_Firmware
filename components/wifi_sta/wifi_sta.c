#include "wifi_sta.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/timers.h"
#include <string.h>
#include <inttypes.h> // Para PRIu32 y otros formatos

// Definiciones
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

// Valores por defecto para la reconexión
#define DEFAULT_RECONNECT_ENABLED true
#define DEFAULT_MAX_RECONNECT_RETRIES 10        // 0 = infinito
#define DEFAULT_INITIAL_RECONNECT_INTERVAL 1000 // 1 segundo
#define DEFAULT_MAX_RECONNECT_INTERVAL 30000    // 30 segundos

static const char *TAG = "WIFI_STA";
static bool s_initialized = false;
static bool s_connected = false;
static bool s_reconnecting = false;
static bool s_ignore_connection_errors = true; // Por defecto ignora errores transitorios
static esp_netif_t *s_wifi_netif = NULL;
static EventGroupHandle_t s_wifi_event_group = NULL;
static TimerHandle_t s_reconnect_timer = NULL;

// Información de conexión actual
static char s_ssid[33] = {0};
static char s_password[65] = {0};
static char s_mac_str[18] = {0};      // MAC con dos puntos
static char s_mac_clean[13] = {0};    // MAC sin dos puntosl y fija

// Configuración de reconexión
static wifi_sta_reconnect_config_t s_reconnect_config = {
    .enabled = DEFAULT_RECONNECT_ENABLED,
    .max_retries = DEFAULT_MAX_RECONNECT_RETRIES,
    .initial_interval_ms = DEFAULT_INITIAL_RECONNECT_INTERVAL,
    .max_interval_ms = DEFAULT_MAX_RECONNECT_INTERVAL};

// Contador de intentos de reconexión
static uint8_t s_reconnect_attempts = 0;
static uint16_t s_current_interval_ms = 0;

esp_err_t sta_wifi_get_mac(char *mac_str, size_t len)
{
    if (!mac_str || len < 18) return ESP_ERR_INVALID_ARG;
    uint8_t mac[6];
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (err != ESP_OK) return err;
    snprintf(mac_str, len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return ESP_OK;
}

// Función de intento de reconexión activada por el timer
static void reconnect_timer_callback(TimerHandle_t timer)
{
    // Si ya estamos conectados o la reconexión está deshabilitada, no hacer nada
    if (s_connected || !s_reconnect_config.enabled)
    {
        s_reconnecting = false;
        return;
    }

    // Verificar si se alcanzó el máximo de intentos (0 = infinito)
    if (s_reconnect_config.max_retries > 0 &&
        s_reconnect_attempts >= s_reconnect_config.max_retries)
    {
        ESP_LOGW(TAG, "Se alcanzó el número máximo de intentos de reconexión (%d)",
                 s_reconnect_config.max_retries);
        s_reconnecting = false;
        return;
    }

    ESP_LOGI(TAG, "Intento de reconexión %d a SSID: %s",
             s_reconnect_attempts + 1, s_ssid);

    // Intentar reconectar
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al iniciar reconexión: %s", esp_err_to_name(ret));
    }

    // Incrementar contador de intentos
    s_reconnect_attempts++;

    // Calculamos el próximo intervalo usando backoff exponencial (duplicar tiempo)
    // con límite superior
    if (s_current_interval_ms < s_reconnect_config.max_interval_ms)
    {
        s_current_interval_ms *= 2;
        if (s_current_interval_ms > s_reconnect_config.max_interval_ms)
        {
            s_current_interval_ms = s_reconnect_config.max_interval_ms;
        }
    }

    // Reiniciar el timer con el nuevo intervalo
    if (s_reconnect_timer != NULL)
    {
        xTimerChangePeriod(s_reconnect_timer, pdMS_TO_TICKS(s_current_interval_ms), 0);
        xTimerReset(s_reconnect_timer, 0);
    }
}

// Inicia el proceso de reconexión
static void start_reconnection_process(void)
{
    // Si la reconexión está deshabilitada, no hacer nada
    if (!s_reconnect_config.enabled)
    {
        return;
    }

    // Si ya estamos en proceso de reconexión, no hacer nada
    if (s_reconnecting)
    {
        return;
    }

    // Si no hay SSID configurado, no podemos reconectar
    if (strlen(s_ssid) == 0)
    {
        ESP_LOGW(TAG, "No hay SSID configurado para reconexión");
        return;
    }

    // Marcar que estamos reconectando
    s_reconnecting = true;
    s_reconnect_attempts = 0;
    s_current_interval_ms = s_reconnect_config.initial_interval_ms;

    ESP_LOGI(TAG, "Iniciando proceso de reconexión automática a SSID: %s", s_ssid);

    // Crear el timer de reconexión si no existe
    if (s_reconnect_timer == NULL)
    {
        s_reconnect_timer = xTimerCreate(
            "wifi_reconnect",
            pdMS_TO_TICKS(s_current_interval_ms),
            pdFALSE, // Auto-reload desactivado (lo configuramos manualmente)
            NULL,
            reconnect_timer_callback);
    }

    // Configurar y lanzar el timer
    if (s_reconnect_timer != NULL)
    {
        xTimerChangePeriod(s_reconnect_timer, pdMS_TO_TICKS(s_current_interval_ms), 0);
        xTimerStart(s_reconnect_timer, 0);
    }
    else
    {
        ESP_LOGE(TAG, "No se pudo crear el timer de reconexión");
        s_reconnecting = false;
    }
}

// Detiene el proceso de reconexión
static void stop_reconnection_process(void)
{
    if (s_reconnect_timer != NULL && s_reconnecting)
    {
        xTimerStop(s_reconnect_timer, 0);
    }
    s_reconnecting = false;
    ESP_LOGI(TAG, "Proceso de reconexión detenido");
}

// Manejadores de eventos
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi iniciado, intentando conectar");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Conectado al AP");
            s_reconnect_attempts = 0; // Resetear contador de intentos
            s_reconnecting = false;   // Ya no estamos reconectando
            if (s_reconnect_timer != NULL)
            {
                xTimerStop(s_reconnect_timer, 0);
            }
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
            s_connected = false;
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);

            // Mostrar información sobre la razón de desconexión
            ESP_LOGW(TAG, "Desconectado del AP, razón: %d", event->reason);

            // Iniciar el proceso de reconexión si está habilitado
            if (s_reconnect_config.enabled && !s_reconnecting)
            {
                start_reconnection_process();
            }
            break;

        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "IP obtenida:" IPSTR, IP2STR(&event->ip_info.ip));
            s_connected = true;
            s_reconnecting = false;
            if (s_reconnect_timer != NULL)
            {
                xTimerStop(s_reconnect_timer, 0);
            }
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

esp_err_t sta_wifi_init(void)
{
    if (s_initialized)
    {
        ESP_LOGI(TAG, "WiFi ya inicializado (idempotente)");
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;

    if (s_initialized)
    {
        ESP_LOGW(TAG, "WiFi ya inicializado");
        return ESP_OK;
    }

    // Crear grupo de eventos para sincronización
    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group)
    {
        ESP_LOGE(TAG, "Fallo al crear grupo de eventos");
        return ESP_FAIL;
    }

    // Inicializar la pila TCP/IP si no está ya inicializada
    ret = esp_netif_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Fallo al inicializar netif: %s", esp_err_to_name(ret));
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
        return ret;
    }

    // Crear el bucle de eventos por defecto si no existe
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Fallo al crear bucle de eventos: %s", esp_err_to_name(ret));
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
        return ret;
    }

    // Crear la interfaz de red para WiFi en modo estación
    s_wifi_netif = esp_netif_create_default_wifi_sta();
    if (!s_wifi_netif)
    {
        ESP_LOGE(TAG, "Fallo al crear netif para WiFi");
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
        return ESP_FAIL;
    }

    // Configurar WiFi con parámetros por defecto
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Fallo al inicializar WiFi: %s", esp_err_to_name(ret));
        esp_netif_destroy(s_wifi_netif);
        s_wifi_netif = NULL;
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
        return ret;
    }

    // Registrar manejadores de eventos
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_event_handler, NULL, NULL);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Fallo al registrar manejador WiFi: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &wifi_event_handler, NULL, NULL);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Fallo al registrar manejador IP: %s", esp_err_to_name(ret));
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, NULL);
        goto cleanup;
    }

    // Configurar WiFi en modo estación (cliente)
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Fallo al configurar modo estación: %s", esp_err_to_name(ret));
        goto cleanup_handlers;
    }

    // Iniciar WiFi
    ret = esp_wifi_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Fallo al iniciar WiFi: %s", esp_err_to_name(ret));
        goto cleanup_handlers;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "WiFi en modo estación inicializado correctamente");
    return ESP_OK;

cleanup_handlers:
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, NULL);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, NULL);

cleanup:
    esp_wifi_deinit();
    esp_netif_destroy(s_wifi_netif);
    s_wifi_netif = NULL;
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;
    return ret;
}

void sta_wifi_deinit(void)
{
    if (!s_initialized)
    {
        ESP_LOGI(TAG, "WiFi ya está deinicializado (idempotente)");
        return;
    }

    if (!s_initialized)
    {
        return;
    }

    // Detener el timer de reconexión si existe
    if (s_reconnect_timer != NULL)
    {
        xTimerStop(s_reconnect_timer, 0);
        xTimerDelete(s_reconnect_timer, 0);
        s_reconnect_timer = NULL;
    }

    // Desconectar primero si está conectado
    if (s_connected)
    {
        sta_wifi_disconnect();
    }

    // Desregistrar handlers de eventos
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, NULL);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, NULL);

    // Detener y deinicializar WiFi
    esp_wifi_stop();
    esp_wifi_deinit();

    // Liberar recursos
    if (s_wifi_netif)
    {
        esp_netif_destroy(s_wifi_netif);
        s_wifi_netif = NULL;
    }

    if (s_wifi_event_group)
    {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    s_initialized = false;
    s_connected = false;
    s_reconnecting = false;

    // Limpiar configuración de conexión
    memset(s_ssid, 0, sizeof(s_ssid));
    memset(s_password, 0, sizeof(s_password));

    ESP_LOGI(TAG, "WiFi en modo estación deinicializado");
}

esp_err_t sta_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms)
{
    if (!s_initialized)
    {
        ESP_LOGE(TAG, "WiFi no inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    if (!ssid || strlen(ssid) == 0)
    {
        ESP_LOGE(TAG, "SSID no válido");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_mac_str[0] == '\0') {
        uint8_t mac[6];
        if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
            snprintf(s_mac_str, sizeof(s_mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            // MAC limpia (sin dos puntos)
            snprintf(s_mac_clean, sizeof(s_mac_clean), "%02X%02X%02X%02X%02X%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    }

    // Limpiar bits de eventos previos
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    // Guardar credenciales para reconexiones futuras
    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
    if (password)
    {
        strncpy(s_password, password, sizeof(s_password) - 1);
    }
    else
    {
        memset(s_password, 0, sizeof(s_password));
    }

    // Configurar WiFi
    wifi_config_t wifi_config = {0};

    // Copiar SSID y password
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);

    if (password)
    {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    // Configurar opciones óptimas para conexiones domésticas
    wifi_config.sta.scan_method = WIFI_FAST_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_config.sta.threshold.rssi = -127;
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

    // Si hay password, exigir al menos WPA2
    if (password && strlen(password) > 0)
    {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    // Aplicar configuración
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al configurar WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Conectando a SSID: %s", ssid);

    // Detener cualquier reconexión en curso
    stop_reconnection_process();

    // Iniciar conexión
    ret = esp_wifi_connect();

    // Mejorado: Si obtenemos ESP_ERR_WIFI_CONN pero estamos configurados para ignorar,
    // no lo tratamos como error fatal si ignore_connection_errors está habilitado
    if (ret == ESP_ERR_WIFI_CONN && s_ignore_connection_errors)
    {
        ESP_LOGW(TAG, "Error ESP_ERR_WIFI_CONN ignorado, continuando con la conexión");
        ret = ESP_OK; // Continuamos normalmente
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al iniciar conexión: %s", esp_err_to_name(ret));
        return ret;
    }

    // Esperar evento de conexión
    EventBits_t bits;

    if (timeout_ms == 0)
    {
        // Esperar indefinidamente
        bits = xEventGroupWaitBits(s_wifi_event_group,
                                   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                   pdFALSE, pdFALSE, portMAX_DELAY);
    }
    else
    {
        // Esperar con timeout
        bits = xEventGroupWaitBits(s_wifi_event_group,
                                   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                   pdFALSE, pdFALSE,
                                   pdMS_TO_TICKS(timeout_ms));
    }

    // Verificar resultado
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Conectado al AP SSID: %s", ssid);
        return ESP_OK;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGE(TAG, "Error al conectar a SSID: %s", ssid);

        // Si falla pero tenemos reconexión automática habilitada, iniciarla
        if (s_reconnect_config.enabled)
        {
            start_reconnection_process();
        }

        return ESP_FAIL;
    }
    else
    {
        ESP_LOGW(TAG, "Tiempo de espera agotado, conexión no completada en %" PRIu32 " ms", timeout_ms);

        // Aunque no recibimos evento en el timeout, la conexión podría seguir intentándose
        // en segundo plano, por lo que no iniciamos un nuevo proceso de reconexión

        return ESP_ERR_TIMEOUT;
    }
}

<<<<<<< HEAD
=======
const char *sta_wifi_get_mac_str(void)
{
    return s_mac_str;
}

const char *sta_wifi_get_mac_clean(void)
{
    return s_mac_clean;
}

/**
 * @brief Conecta al WiFi usando credenciales almacenadas en NVS
 * 
 * @param timeout_ms Tiempo de espera en ms (0 = esperar indefinidamente)
 * @return esp_err_t ESP_OK si se conecta correctamente, 
 *                   ESP_ERR_NOT_FOUND si no hay credenciales ni en NVS ni en Kconfig
 */
esp_err_t sta_wifi_connect_with_nvs(uint32_t timeout_ms)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "WiFi no inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    // Si ya estamos conectados, no hacemos nada (idempotente)
    if (s_connected) {
        ESP_LOGI(TAG, "Ya conectado a WiFi (idempotente)");
        return ESP_OK;
    }

    // Verificar si NVS está inicializado
    if (!nvs_manager_is_initialized()) {
        ESP_LOGW(TAG, "NVS no está inicializado, inicializando...");
        esp_err_t ret = nvs_manager_init(NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error al inicializar NVS Manager: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    
    // Buffers para las credenciales
    char nvs_ssid[33] = {0};
    char nvs_password[65] = {0};
    
    // Primero intentar con credenciales de NVS
    if (nvs_manager_has_wifi_credentials()) {
        // Obtener credenciales de NVS
        esp_err_t ret = nvs_manager_get_wifi_credentials(nvs_ssid, sizeof(nvs_ssid), 
                                                        nvs_password, sizeof(nvs_password));
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Conectando con credenciales de NVS. SSID: %s", nvs_ssid);
            return sta_wifi_connect(nvs_ssid, 
                                  (nvs_password[0] != '\0' ? nvs_password : NULL), 
                                  timeout_ms);
        } else {
            ESP_LOGW(TAG, "Error al obtener credenciales de NVS: %s", esp_err_to_name(ret));
            // Continuamos para intentar con Kconfig
        }
    } else {
        ESP_LOGW(TAG, "No hay credenciales WiFi almacenadas en NVS");
    }
    
    // Si llegamos aquí, no hay credenciales en NVS o hubo error al leerlas
    // Intentar usar credenciales de Kconfig
    #if defined(CONFIG_WIFI_STA_SSID)
        const char *kconfig_ssid = CONFIG_WIFI_STA_SSID;
        
        // Verificar en tiempo de ejecución si el SSID es válido
        if (kconfig_ssid != NULL && kconfig_ssid[0] != '\0') {
            const char *kconfig_password = NULL;
            
            #if defined(CONFIG_WIFI_STA_PASSWORD) && !defined(CONFIG_WIFI_STA_OPEN_NETWORK)
                kconfig_password = CONFIG_WIFI_STA_PASSWORD;
            #endif
            
            ESP_LOGI(TAG, "Usando credenciales WiFi de Kconfig. SSID: %s", kconfig_ssid);
            return sta_wifi_connect(kconfig_ssid, kconfig_password, timeout_ms);
        } else {
            ESP_LOGW(TAG, "SSID de Kconfig está vacío");
        }
    #endif

    // Si llegamos aquí, no hay credenciales válidas
    ESP_LOGW(TAG, "No hay credenciales WiFi configuradas ni en NVS ni en Kconfig");
    return ESP_ERR_NOT_FOUND;
}

>>>>>>> CorregirWifi
esp_err_t sta_wifi_disconnect(void)
{
    if (!s_initialized)
    {
        ESP_LOGE(TAG, "WiFi no inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    // Si ya estamos desconectados, no hacemos nada (idempotente)
    if (!s_connected)
    {
        ESP_LOGI(TAG, "WiFi ya está desconectado (idempotente)");
        return ESP_OK;
    }

    // Detener cualquier proceso de reconexión
    stop_reconnection_process();

    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al desconectar WiFi: %s", esp_err_to_name(ret));
    }
    else
    {
        s_connected = false;
        ESP_LOGI(TAG, "Desconectado correctamente");
    }

    return ret;
}

esp_err_t sta_wifi_configure_reconnect(const wifi_sta_reconnect_config_t *config)
{
    if (config == NULL)
    {
        // Usar configuración por defecto
        s_reconnect_config.enabled = DEFAULT_RECONNECT_ENABLED;
        s_reconnect_config.max_retries = DEFAULT_MAX_RECONNECT_RETRIES;
        s_reconnect_config.initial_interval_ms = DEFAULT_INITIAL_RECONNECT_INTERVAL;
        s_reconnect_config.max_interval_ms = DEFAULT_MAX_RECONNECT_INTERVAL;
    }
    else
    {
        // Usar configuración proporcionada
        s_reconnect_config = *config;
    }

    ESP_LOGI(TAG, "Configuración de reconexión: habilitado=%d, max_intentos=%d, intervalo_inicial=%d ms, intervalo_max=%d ms",
             s_reconnect_config.enabled, s_reconnect_config.max_retries,
             s_reconnect_config.initial_interval_ms, s_reconnect_config.max_interval_ms);

    return ESP_OK;
}

void sta_wifi_enable_reconnect(bool enable)
{
    s_reconnect_config.enabled = enable;

    if (!enable)
    {
        // Si estamos deshabilitando, detener cualquier reconexión en curso
        stop_reconnection_process();
    }

    ESP_LOGI(TAG, "Reconexión automática %s", enable ? "habilitada" : "deshabilitada");
}

bool sta_wifi_is_connected(void)
{
    return s_connected;
}

esp_err_t sta_wifi_get_ip(char *ip_str)
{
    if (!s_initialized || !s_connected || !ip_str)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(s_wifi_netif, &ip_info);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error obteniendo información IP: %s", esp_err_to_name(ret));
        return ret;
    }

    // Convertir IP a string
    sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}

esp_err_t sta_wifi_get_rssi(int8_t *rssi)
{
    if (!s_initialized || !s_connected || !rssi)
    {
        return ESP_ERR_INVALID_STATE;
    }

    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error obteniendo información AP: %s", esp_err_to_name(ret));
        return ret;
    }

    *rssi = ap_info.rssi;
    return ESP_OK;
}

bool sta_wifi_is_reconnecting(void)
{
    return s_reconnecting;
}

void sta_wifi_set_ignore_connection_errors(bool ignore_connection_errors)
{
    s_ignore_connection_errors = ignore_connection_errors;
    ESP_LOGI(TAG, "Ignorar errores de conexión temporales: %s",
             ignore_connection_errors ? "activado" : "desactivado");
}

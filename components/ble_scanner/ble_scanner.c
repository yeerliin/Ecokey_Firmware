// ble_scanner.c
#include "ble_scanner.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_bt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "esp_nimble_hci.h"
#include <ctype.h>
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"
// Incluir cabeceras para el sensor de temperatura
#include "driver/temperature_sensor.h"
#include "esp_check.h"
#include <math.h>
// Añade el include del servicio MQTT y el de la función para obtener la MAC limpia
#include "mqtt_service.h"
#include "wifi_sta.h"

static const char *TAG = "BLE_SCANNER";

// Mutex para proteger accesos concurrentes
static portMUX_TYPE s_ble_mux = portMUX_INITIALIZER_UNLOCKED;

// Estructura para almacenar información de MACs objetivo
typedef struct
{
    uint8_t mac[6];
    uint32_t mac_hash; // Hash para comparación rápida
    bool en_uso;
    bool detectado; // Flag normal (protegido por mutex)
} ble_scanner_target_t;

// Estructura para información de detección en la cola
typedef struct
{
    uint8_t target_idx;
    uint8_t rssi;
} detection_info_t;

// Variables estáticas del módulo
static ble_scanner_target_t s_targets[BLE_SCANNER_MAX_TARGET_DEVICES] = {0};
static ble_scanner_config_t s_config = BLE_SCANNER_DEFAULT_CONFIG();
static bool s_inicializado = false;
static bool s_escaneo_activo = false;
static bool s_host_sincronizado = false;       // Variable para controlar la sincronización
static QueueHandle_t s_detection_queue = NULL; // Cola para procesar detecciones

// Variables para control térmico
static bool s_control_termico_activo = true;
static float s_temp_warning = BLE_SCANNER_TEMP_WARNING;
static float s_temp_critical = BLE_SCANNER_TEMP_CRITICAL;
static float s_temperatura_actual = 0.0f;
static ble_thermal_mode_t s_modo_termico = BLE_THERMAL_MODE_NORMAL;
static temperature_sensor_handle_t s_temp_sensor = NULL;
static TaskHandle_t s_temp_task_handle = NULL;
static struct ble_gap_disc_params s_scan_params_normal = {0};
static struct ble_gap_disc_params s_scan_params_warning = {0};

// Prototipos de funciones internas
static void detection_task(void *param);
static void crear_mac_hash(uint8_t target_idx);
static void temp_monitor_task(void *param);
static esp_err_t inicializar_sensor_temperatura(void);
static float leer_temperatura(void);
static void aplicar_modo_termico(ble_thermal_mode_t nuevo_modo);
static esp_err_t iniciar_escaneo_con_modo(ble_thermal_mode_t modo);

/**
 * Calcula un hash simple para la MAC para comparación rápida
 */
static void crear_mac_hash(uint8_t target_idx)
{
    const uint8_t *mac = s_targets[target_idx].mac;
    // Crear un hash simple con los primeros 4 bytes para comparación rápida
    s_targets[target_idx].mac_hash = ((uint32_t)mac[0] << 24) |
                                     ((uint32_t)mac[1] << 16) |
                                     ((uint32_t)mac[2] << 8) |
                                     mac[3];
}

/**
 * Callback de escaneo BLE optimizado: se llama cada vez que se detecta un dispositivo.
 * Usa IRAM_ATTR para ejecución rápida desde RAM interna.
 */
static IRAM_ATTR int ble_app_scan_cb(struct ble_gap_event *event, void *arg)
{
    if (event->type == BLE_GAP_EVENT_DISC)
    {
        const uint8_t *adv_mac = event->disc.addr.val;

        // Crear hash de los primeros 4 bytes de la MAC recibida para comparación rápida
        uint32_t adv_hash = ((uint32_t)adv_mac[0] << 24) |
                            ((uint32_t)adv_mac[1] << 16) |
                            ((uint32_t)adv_mac[2] << 8) |
                            adv_mac[3];

        // Comparar con todas las MACs objetivo
        for (int target_idx = 0; target_idx < BLE_SCANNER_MAX_TARGET_DEVICES; target_idx++)
        {
            if (!s_targets[target_idx].en_uso)
            {
                continue; // Saltar posiciones no utilizadas
            }

            // Primero comparar hash para descartar rápidamente (optimización)
            if (adv_hash != s_targets[target_idx].mac_hash)
            {
                continue;
            }

            // Comparación completa solo si el hash coincide
            bool match = true;
            for (int i = 0; i < 6; i++)
            {
                if (adv_mac[i] != s_targets[target_idx].mac[i])
                {
                    match = false;
                    break;
                }
            }

            if (match)
            {
                // Marcar como detectado usando sección crítica
                portENTER_CRITICAL(&s_ble_mux);
                s_targets[target_idx].detectado = true;
                portEXIT_CRITICAL(&s_ble_mux);

                // Quitar logs aquí, solo enviar a la cola
                if (s_detection_queue != NULL)
                {
                    detection_info_t info = {
                        .target_idx = target_idx,
                        .rssi = event->disc.rssi};
                    xQueueSendFromISR(s_detection_queue, &info, NULL);
                }
            }
        }
    }
    return 0;
}

/**
 * Tarea para procesar detecciones fuera del contexto del callback
 */
static void detection_task(void *param)
{
    detection_info_t info;
    static bool tag_reportado[BLE_SCANNER_MAX_TARGET_DEVICES] = {0};

    while (1)
    {
        if (xQueueReceive(s_detection_queue, &info, portMAX_DELAY))
        {
            // Solo loguear la PRIMERA vez que se detecta el tag
            if (!tag_reportado[info.target_idx]) {
                tag_reportado[info.target_idx] = true;
                ESP_LOGI(TAG, "Tag #%d detectado! MAC: %02X:%02X:%02X:%02X:%02X:%02X RSSI:%d",
                         info.target_idx,
                         s_targets[info.target_idx].mac[5], s_targets[info.target_idx].mac[4],
                         s_targets[info.target_idx].mac[3], s_targets[info.target_idx].mac[2],
                         s_targets[info.target_idx].mac[1], s_targets[info.target_idx].mac[0],
                         info.rssi);
            }
            // Si quieres volver a permitir el log tras perder el tag, puedes resetear el flag
        }
    }
}

/**
 * Inicializa el sensor de temperatura interno
 */
static esp_err_t inicializar_sensor_temperatura(void)
{
    ESP_LOGI(TAG, "Inicializando sensor de temperatura...");

    temperature_sensor_config_t temp_sensor_config = {
        .range_min = 10,
        .range_max = 80,
        .clk_src = TEMPERATURE_SENSOR_CLK_SRC_DEFAULT
    };

    ESP_RETURN_ON_ERROR(temperature_sensor_install(&temp_sensor_config, &s_temp_sensor),
                       TAG, "Error al instalar sensor de temperatura");
    ESP_RETURN_ON_ERROR(temperature_sensor_enable(s_temp_sensor),
                       TAG, "Error al habilitar sensor de temperatura");

    ESP_LOGI(TAG, "Sensor de temperatura inicializado correctamente");
    return ESP_OK;
}

/**
 * Lee la temperatura actual del chip
 */
static float leer_temperatura(void)
{
    float temp = 0;
    if (s_temp_sensor != NULL) {
        if (temperature_sensor_get_celsius(s_temp_sensor, &temp) != ESP_OK) {
            ESP_LOGW(TAG, "Error al leer temperatura, usando último valor conocido");
            return s_temperatura_actual; // Retornar último valor conocido en caso de error
        }
    } else {
        ESP_LOGW(TAG, "Sensor de temperatura no inicializado");
        return 25.0f; // Valor predeterminado si no hay sensor
    }
    return temp;
}

/**
 * Tarea para monitorear la temperatura periódicamente
 */
static void temp_monitor_task(void *param)
{
    ESP_LOGI(TAG, "Tarea de monitoreo térmico iniciada");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    float last_reported_temp = -1000.0f; // Valor imposible para forzar primer log
    TickType_t last_sent_tick = 0;
    const TickType_t min_send_interval = pdMS_TO_TICKS(300000); // 5 minutos

    // Buffer para topic MQTT
    char temp_topic[64] = {0};
    // Obtener la MAC limpia una sola vez (asume que no cambia en runtime)
    const char *mac_clean = sta_wifi_get_mac_clean();
    snprintf(temp_topic, sizeof(temp_topic), "dispositivos/%s/temp", mac_clean);

    while (1) {
        // Leer temperatura actual
        s_temperatura_actual = leer_temperatura();

        // Determinar modo térmico basado en la temperatura
        ble_thermal_mode_t nuevo_modo = BLE_THERMAL_MODE_NORMAL;

        if (s_temperatura_actual >= s_temp_critical) {
            nuevo_modo = BLE_THERMAL_MODE_CRITICAL;
        } else if (s_temperatura_actual >= s_temp_warning) {
            nuevo_modo = BLE_THERMAL_MODE_WARNING;
        }

        // Aplicar cambios de modo si es necesario
        if (s_control_termico_activo && nuevo_modo != s_modo_termico) {
            ESP_LOGI(TAG, "Cambio de modo térmico: %d -> %d (Temperatura: %.1f°C)",
                     s_modo_termico, nuevo_modo, s_temperatura_actual);
            aplicar_modo_termico(nuevo_modo);
        }

        // Reportar temperatura solo si cambia al menos 2 grados, pasó 5 minutos desde el último envío, o hay un cambio crítico
        TickType_t now = xTaskGetTickCount();

        bool cambio_critico = (s_temperatura_actual >= s_temp_warning && last_reported_temp < s_temp_warning) ||
                              (s_temperatura_actual >= s_temp_critical && last_reported_temp < s_temp_critical);

        if (fabsf(s_temperatura_actual - last_reported_temp) >= 2.0f ||
            (now - last_sent_tick) > min_send_interval ||
            cambio_critico) {
            char temp_json[48];
            snprintf(temp_json, sizeof(temp_json), "{\"temperatura\":%.1f}", s_temperatura_actual);
            mqtt_service_enviar_dato(temp_topic, temp_json, 2, 1);
            last_reported_temp = s_temperatura_actual;
            last_sent_tick = now;
        }

        // Esperar 500ms antes de la próxima lectura
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
    }
}

/**
 * Aplica cambios basados en el modo térmico
 */
static void aplicar_modo_termico(ble_thermal_mode_t nuevo_modo)
{
    if (!s_inicializado || !s_control_termico_activo) {
        return;
    }
    
    s_modo_termico = nuevo_modo;
    
    switch (nuevo_modo) {
        case BLE_THERMAL_MODE_NORMAL:
            ESP_LOGI(TAG, "Modo térmico NORMAL: Escaneo BLE a velocidad completa");
            if (s_escaneo_activo) {
                ble_scanner_detener();
                iniciar_escaneo_con_modo(BLE_THERMAL_MODE_NORMAL);
            }
            break;
            
        case BLE_THERMAL_MODE_WARNING:
            ESP_LOGW(TAG, "Modo térmico ADVERTENCIA: Reduciendo actividad BLE");
            if (s_escaneo_activo) {
                ble_scanner_detener();
                iniciar_escaneo_con_modo(BLE_THERMAL_MODE_NORMAL);
            }
            break;
            
        case BLE_THERMAL_MODE_CRITICAL:
            ESP_LOGE(TAG, "Modo térmico CRÍTICO: Deteniendo escaneo BLE");
            if (s_escaneo_activo) {
                ble_scanner_detener();
                iniciar_escaneo_con_modo(BLE_THERMAL_MODE_WARNING);
                // En modo crítico, no reiniciamos el escaneo
            }
            break;
    }
}

/**
 * Inicia el escaneo BLE con parámetros adaptados al modo térmico actual
 */
static esp_err_t iniciar_escaneo_con_modo(ble_thermal_mode_t modo)
{
    if (!s_inicializado) {
        ESP_LOGE(TAG, "No se puede iniciar escaneo, BLE no inicializado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (modo == BLE_THERMAL_MODE_CRITICAL) {
        ESP_LOGE(TAG, "No se puede iniciar escaneo en modo térmico crítico");
        return ESP_ERR_INVALID_STATE;
    }
    
    struct ble_gap_disc_params *scan_params = &s_scan_params_normal;
    
    if (modo == BLE_THERMAL_MODE_WARNING) {
        // En modo de advertencia, usar parámetros de ahorro térmico
        scan_params = &s_scan_params_warning;
        ESP_LOGI(TAG, "Iniciando escaneo BLE en modo térmico de advertencia");
    } else {
        ESP_LOGI(TAG, "Iniciando escaneo BLE en modo térmico normal");
    }
    
    int duration_ticks = BLE_HS_FOREVER;
    if (s_config.duration_ms > 0) {
        duration_ticks = s_config.duration_ms / 10; // Convertir ms a ticks BLE
    }
    
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, duration_ticks, scan_params, ble_app_scan_cb, NULL);
    
    if (rc != 0) {
        ESP_LOGE(TAG, "Fallo al iniciar escaneo: %d", rc);
        return ESP_FAIL;
    }
    
    s_escaneo_activo = true;
    ESP_LOGI(TAG, "Escaneo BLE iniciado con parámetros para modo térmico %d", modo);
    return ESP_OK;
}

/**
 * Configura y lanza escaneo BLE con los parámetros actuales.
 */
static esp_err_t iniciar_escaneo(void)
{
    if (s_control_termico_activo && s_modo_termico == BLE_THERMAL_MODE_CRITICAL) {
        ESP_LOGW(TAG, "No se inicia escaneo debido a modo térmico crítico");
        return ESP_ERR_INVALID_STATE;
    }
    
    return iniciar_escaneo_con_modo(s_modo_termico);
}

// Callback para cuando el host BLE está sincronizado
static void on_ble_host_sync(void)
{
    ESP_LOGI(TAG, "Host BLE sincronizado, listo para iniciar escaneo");
    s_host_sincronizado = true; // Marcar que el host ya está sincronizado

    // Iniciar escaneo solo si ya estamos inicializados
    if (s_inicializado)
    {
        esp_err_t ret = iniciar_escaneo();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Error al iniciar escaneo automático: %s", esp_err_to_name(ret));
        }
    }
    else
    {
        ESP_LOGW(TAG, "Host sincronizado pero escáner no inicializado completamente, postergando escaneo");
    }
}

/**
 * Ejecuta el stack BLE NimBLE en FreeRTOS.
 */
static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task iniciada");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/**
 * Inicialización del escáner BLE con la configuración proporcionada.
 */
esp_err_t ble_scanner_iniciar(const ble_scanner_config_t *config)
{
    // Verificar si ya está inicializado
    if (s_inicializado)
    {
        ESP_LOGW(TAG, "El escáner BLE ya está inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    // Aplicar configuración personalizada o usar valores por defecto
    if (config != NULL)
    {
        memcpy(&s_config, config, sizeof(ble_scanner_config_t));
        s_control_termico_activo = config->control_termico;
        s_temp_warning = config->temp_warning;
        s_temp_critical = config->temp_critical;
    }
    else
    {
        s_config = (ble_scanner_config_t)BLE_SCANNER_DEFAULT_CONFIG();
        s_control_termico_activo = true;
        s_temp_warning = BLE_SCANNER_TEMP_WARNING;
        s_temp_critical = BLE_SCANNER_TEMP_CRITICAL;
    }

    ESP_LOGI(TAG, "Inicializando BLE Scanner...");

    // Inicializar el sensor de temperatura
    esp_err_t ret = inicializar_sensor_temperatura();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Error al inicializar sensor de temperatura: %s. Continuando sin control térmico.", 
                 esp_err_to_name(ret));
        s_control_termico_activo = false;
    }

    // Configurar parámetros de escaneo para modos normal y de advertencia
    // Modo normal - parámetros completos
    s_scan_params_normal.passive = s_config.passive;
    s_scan_params_normal.itvl = s_config.scan_interval;
    s_scan_params_normal.window = s_config.scan_window;
    s_scan_params_normal.filter_duplicates = s_config.filter_duplicates;
    
    // Modo advertencia - menor duty cycle para reducir calentamiento (ventana 1/3 del intervalo)
    s_scan_params_warning.passive = true; // Siempre pasivo en modo de advertencia
    s_scan_params_warning.itvl = s_config.scan_interval * 3; // Intervalo mayor
    s_scan_params_warning.window = s_config.scan_window; // Misma ventana
    s_scan_params_warning.filter_duplicates = s_config.filter_duplicates;

    // Crear tarea de monitoreo térmico si está activo
    if (s_control_termico_activo) {
        BaseType_t res = xTaskCreate(
            temp_monitor_task,
            "temp_monitor",
            3072,
            NULL,
            3, // Prioridad media
            &s_temp_task_handle);
        
        if (res != pdPASS) {
            ESP_LOGE(TAG, "Error al crear tarea de monitoreo térmico");
            // Continuamos, esto no es un error fatal
        }
    }

    // Resetear variables de estado
    s_host_sincronizado = false;
    s_modo_termico = BLE_THERMAL_MODE_NORMAL;

    // Crear cola para procesamiento de detecciones
    if (s_detection_queue == NULL)
    {
        s_detection_queue = xQueueCreate(10, sizeof(detection_info_t));
        if (s_detection_queue == NULL)
        {
            ESP_LOGE(TAG, "Error al crear cola de detecciones");
            return ESP_ERR_NO_MEM;
        }

        // Crear tarea para procesar detecciones
        BaseType_t res = xTaskCreate(
            detection_task,
            "ble_detection",
            4096, // <-- Aumentar stack de 2048 a 4096 bytes
            NULL,
            5,
            NULL);

        if (res != pdPASS)
        {
            ESP_LOGE(TAG, "Error al crear tarea de detección");
            vQueueDelete(s_detection_queue);
            s_detection_queue = NULL;
            return ESP_FAIL;
        }
    }

    // Configurar callback para cuando el host esté listo
    ble_hs_cfg.sync_cb = on_ble_host_sync;

    // Inicializar NimBLE
    ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al inicializar NimBLE: %d", ret);
        return ESP_FAIL;
    }

    // Iniciar la tarea del host BLE con prioridad alta para callback rápido
    nimble_port_freertos_init(ble_host_task);

    // Ahora sí marcamos como inicializado
    s_inicializado = true;

    // Si el host ya se sincronizó (poco probable pero posible),
    // iniciar el escaneo inmediatamente
    if (s_host_sincronizado)
    {
        ESP_LOGI(TAG, "Host ya sincronizado, iniciando escaneo inmediatamente");
        ret = iniciar_escaneo();
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Error al iniciar escaneo, se intentará más tarde: %s",
                     esp_err_to_name(ret));
            // No consideramos esto un error fatal
        }
    }

    ESP_LOGI(TAG, "BLE scanner inicializado con éxito");
    ESP_LOGI(TAG, "Control térmico: %s (Warning: %.1f°C, Critical: %.1f°C)",
             s_control_termico_activo ? "Activado" : "Desactivado",
             s_temp_warning, s_temp_critical);
    return ESP_OK;
}

esp_err_t ble_scanner_deinicializar(void)
{
    if (!s_inicializado)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // Detener el escaneo si está activo
    if (s_escaneo_activo)
    {
        ble_scanner_detener();
    }

    // Eliminar la tarea de monitoreo de temperatura
    if (s_temp_task_handle != NULL) {
        vTaskDelete(s_temp_task_handle);
        s_temp_task_handle = NULL;
    }
    
    // Deshabilitar y desinstalar el sensor de temperatura
    if (s_temp_sensor != NULL) {
        temperature_sensor_disable(s_temp_sensor);
        temperature_sensor_uninstall(s_temp_sensor);
        s_temp_sensor = NULL;
    }

    // Eliminar cola y recursos de detección
    if (s_detection_queue != NULL)
    {
        vQueueDelete(s_detection_queue);
        s_detection_queue = NULL;
    }

    // Detener y deinicializar NimBLE
    nimble_port_stop();
    nimble_port_deinit();

    s_inicializado = false;
    ESP_LOGI(TAG, "BLE scanner deinicializado");
    return ESP_OK;
}

esp_err_t ble_scanner_detener(void)
{
    if (!s_inicializado)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_escaneo_activo)
    {
        return ESP_OK;
    }

    int rc = ble_gap_disc_cancel();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error al detener el escaneo: %d", rc);
        return ESP_FAIL;
    }

    s_escaneo_activo = false;
    ESP_LOGI(TAG, "Escaneo BLE detenido");
    return ESP_OK;
}

esp_err_t ble_scanner_reiniciar(void)
{
    esp_err_t ret = ble_scanner_detener();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        return ret;
    }

    return iniciar_escaneo();
}

bool ble_scanner_tag_detectado(uint8_t mac_index)
{
    if (mac_index >= BLE_SCANNER_MAX_TARGET_DEVICES || !s_targets[mac_index].en_uso)
    {
        return false;
    }

    // Usar sección crítica para leer y resetear el flag en una única operación atómica
    bool detectado;
    portENTER_CRITICAL(&s_ble_mux);
    detectado = s_targets[mac_index].detectado;
    s_targets[mac_index].detectado = false;
    portEXIT_CRITICAL(&s_ble_mux);

    return detectado;
}

bool ble_scanner_cualquier_tag_detectado(void)
{
    bool detectado = false;

    portENTER_CRITICAL(&s_ble_mux);
    for (int i = 0; i < BLE_SCANNER_MAX_TARGET_DEVICES; i++)
    {
        if (s_targets[i].en_uso && s_targets[i].detectado)
        {
            detectado = true;
            // No reseteamos los flags individuales
            break;
        }
    }
    portEXIT_CRITICAL(&s_ble_mux);

    return detectado;
}

esp_err_t ble_scanner_configurar_mac_objetivo(uint8_t mac_index, const uint8_t *mac)
{
    if (mac_index >= BLE_SCANNER_MAX_TARGET_DEVICES)
    {
        ESP_LOGE(TAG, "Índice de MAC inválido: %d", mac_index);
        return ESP_ERR_INVALID_ARG;
    }

    if (mac == NULL)
    {
        ESP_LOGE(TAG, "Puntero a MAC es NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Almacenar MAC en orden invertido para comparación directa
    for (int i = 0; i < 6; i++)
    {
        s_targets[mac_index].mac[i] = mac[5 - i];
    }

    portENTER_CRITICAL(&s_ble_mux);
    s_targets[mac_index].en_uso = true;
    s_targets[mac_index].detectado = false;
    portEXIT_CRITICAL(&s_ble_mux);

    // Generar hash para comparación rápida
    crear_mac_hash(mac_index);

    ESP_LOGI(TAG, "MAC objetivo #%d configurada: %02X:%02X:%02X:%02X:%02X:%02X",
             mac_index, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return ESP_OK;
}

esp_err_t ble_scanner_configurar_mac_objetivo_texto(uint8_t mac_index, const char *mac_str)
{
    if (mac_index >= BLE_SCANNER_MAX_TARGET_DEVICES)
    {
        ESP_LOGE(TAG, "Índice de MAC inválido: %d", mac_index);
        return ESP_ERR_INVALID_ARG;
    }

    if (mac_str == NULL)
    {
        ESP_LOGE(TAG, "Puntero a string MAC es NULL");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t mac[6] = {0};

    size_t len = strlen(mac_str);
    if (len == 12)
    {
        // Formato plano sin separadores
        for (int i = 0; i < 6; i++)
        {
            char byte_str[3] = {mac_str[i * 2], mac_str[i * 2 + 1], 0};
            mac[i] = (uint8_t)strtol(byte_str, NULL, 16);
        }
    }
    else if (len == 17)
    {
        // Formato con separadores (':')
        for (int i = 0; i < 6; i++)
        {
            char byte_str[3] = {mac_str[i * 3], mac_str[i * 3 + 1], 0};
            mac[i] = (uint8_t)strtol(byte_str, NULL, 16);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Formato de MAC inválido: %s", mac_str);
        return ESP_ERR_INVALID_ARG;
    }
    // Añadir estos logs
    ESP_LOGI(TAG, "MAC a buscar (original): %s", mac_str);
    ESP_LOGI(TAG, "MAC convertida: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return ble_scanner_configurar_mac_objetivo(mac_index, mac);
}

esp_err_t ble_scanner_limpiar_macs_objetivo(void)
{
    portENTER_CRITICAL(&s_ble_mux);
    memset(s_targets, 0, sizeof(s_targets));
    portEXIT_CRITICAL(&s_ble_mux);

    ESP_LOGI(TAG, "Lista de MACs objetivo limpiada");
    return ESP_OK;
}

bool ble_scanner_esta_activo(void)
{
    return s_escaneo_activo;
}

// Nuevas funciones para el control térmico
float ble_scanner_obtener_temperatura(void)
{
    return s_temperatura_actual;
}

ble_thermal_mode_t ble_scanner_obtener_modo_termico(void)
{
    return s_modo_termico;
}

esp_err_t ble_scanner_configurar_umbrales_temperatura(float temp_warning, float temp_critical)
{
    if (temp_warning >= temp_critical || temp_warning < 10.0f || temp_critical > 80.0f) {
        ESP_LOGE(TAG, "Umbrales de temperatura inválidos (warning: %.1f, critical: %.1f)", 
                 temp_warning, temp_critical);
        return ESP_ERR_INVALID_ARG;
    }
    
    s_temp_warning = temp_warning;
    s_temp_critical = temp_critical;
    
    ESP_LOGI(TAG, "Umbrales de temperatura actualizados: Warning=%.1f°C, Critical=%.1f°C", 
             s_temp_warning, s_temp_critical);
    return ESP_OK;
}

esp_err_t ble_scanner_activar_control_termico(bool activar)
{
    s_control_termico_activo = activar;
    
    if (activar) {
        ESP_LOGI(TAG, "Control térmico activado");
        
        // Si no hay tarea de monitoreo, crearla
        if (s_temp_task_handle == NULL && s_inicializado) {
            BaseType_t res = xTaskCreate(
                temp_monitor_task,
                "temp_monitor",
                3072,
                NULL,
                3,
                &s_temp_task_handle);
                
            if (res != pdPASS) {
                ESP_LOGE(TAG, "Error al crear tarea de monitoreo térmico");
                return ESP_FAIL;
            }
        }
    } else {
        ESP_LOGI(TAG, "Control térmico desactivado");
        
        // Si estamos en un modo de protección térmica, restaurar operación normal
        if (s_modo_termico != BLE_THERMAL_MODE_NORMAL && s_inicializado) {
            s_modo_termico = BLE_THERMAL_MODE_NORMAL;
            
            // Reiniciar escaneo con parámetros normales si estaba activo
            if (s_escaneo_activo) {
                ble_scanner_detener();
                iniciar_escaneo();
            }
        }
    }
    
    return ESP_OK;
}
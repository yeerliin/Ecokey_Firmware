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

// Prototipos de funciones internas
static void detection_task(void *param);
static void crear_mac_hash(uint8_t target_idx);

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

        // Quitar logs aquí, NO usar ESP_LOG* en IRAM_ATTR ni en ISR
        // ESP_LOGI(TAG, "MAC detectada: ...");

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
            if (!tag_reportado[info.target_idx])
            {
                tag_reportado[info.target_idx] = true;
                ESP_LOGI(TAG, "MAC detectada: #%d con RSSI: %d", info.target_idx, info.rssi);
                ESP_LOGI(TAG, "✅ Tag #%d detectado! MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                         info.target_idx,
                         s_targets[info.target_idx].mac[5], s_targets[info.target_idx].mac[4],
                         s_targets[info.target_idx].mac[3], s_targets[info.target_idx].mac[2],
                         s_targets[info.target_idx].mac[1], s_targets[info.target_idx].mac[0]);
            }
            // Si quieres volver a permitir el log tras perder el tag, puedes resetear el flag
            // desde otra función/tarea cuando el tag deje de estar presente.
        }
    }
}

/**
 * Configura y lanza escaneo BLE con los parámetros actuales.
 */
static esp_err_t iniciar_escaneo(void)
{
    if (!s_inicializado)
    {
        ESP_LOGE(TAG, "No se puede iniciar escaneo, BLE no inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    struct ble_gap_disc_params scan_params = {0};
    scan_params.passive = s_config.passive;
    scan_params.itvl = s_config.scan_interval;
    scan_params.window = s_config.scan_window;
    scan_params.filter_duplicates = s_config.filter_duplicates;

    int duration_ticks = BLE_HS_FOREVER;
    if (s_config.duration_ms > 0)
    {
        duration_ticks = s_config.duration_ms / 10; // Convertir ms a ticks BLE
    }

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, duration_ticks,
                          &scan_params, ble_app_scan_cb, NULL);

    if (rc != 0)
    {
        ESP_LOGE(TAG, "Fallo al iniciar escaneo: %d", rc);
        return ESP_FAIL;
    }

    s_escaneo_activo = true;
    ESP_LOGI(TAG, "Escaneo BLE iniciado");
    return ESP_OK;
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

    //  Recomendado: liberar memoria de BT Classic si aún no se ha hecho
    esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Error al liberar memoria BT Classic: %s", esp_err_to_name(ret));
        return ret;
    }

    // Aplicar configuración personalizada o usar valores por defecto
    if (config != NULL)
    {
        memcpy(&s_config, config, sizeof(ble_scanner_config_t));
    }
    else
    {
        s_config = (ble_scanner_config_t)BLE_SCANNER_DEFAULT_CONFIG();
    }

    ESP_LOGI(TAG, "Inicializando BLE Scanner...");

    s_host_sincronizado = false;

    if (s_detection_queue == NULL)
    {
        s_detection_queue = xQueueCreate(10, sizeof(detection_info_t));
        if (s_detection_queue == NULL)
        {
            ESP_LOGE(TAG, "Error al crear cola de detecciones");
            return ESP_ERR_NO_MEM;
        }

        BaseType_t res = xTaskCreate(
            detection_task,
            "ble_detection",
            4096,
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

    ble_hs_cfg.sync_cb = on_ble_host_sync;

    ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al inicializar NimBLE: %d", ret);
        return ESP_FAIL;
    }

    nimble_port_freertos_init(ble_host_task);

    s_inicializado = true;

    if (s_host_sincronizado)
    {
        ESP_LOGI(TAG, "Host ya sincronizado, iniciando escaneo inmediatamente");
        ret = iniciar_escaneo();
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Error al iniciar escaneo, se intentará más tarde: %s",
                     esp_err_to_name(ret));
        }
    }

    ESP_LOGI(TAG, "BLE scanner inicializado con éxito");
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
        ESP_LOGW(TAG, "El escáner BLE no está activo actualmente");
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
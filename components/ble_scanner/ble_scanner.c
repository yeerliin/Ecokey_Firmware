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
#include "driver/temperature_sensor.h"
#include "esp_check.h"
#include <math.h>
#include "mqtt_service.h"
#include "wifi_sta.h"

static const char *TAG = "BLE_SCANNER_S3";

// Mutex para protección concurrente
static portMUX_TYPE s_ble_mux = portMUX_INITIALIZER_UNLOCKED;

// Estructura para MACs objetivo
typedef struct {
    uint8_t mac[6];
    uint32_t mac_hash;
    bool en_uso;
    bool detectado;
    uint32_t detecciones_totales;
    int64_t ultima_deteccion;
} ble_scanner_target_t;

// Estructura para información de detección
typedef struct {
    uint8_t target_idx;
    int8_t rssi;
    int64_t timestamp;
} detection_info_t;

// Variables del módulo
static ble_scanner_target_t s_targets[BLE_SCANNER_MAX_TARGET_DEVICES] = {0};
static ble_scanner_config_t s_config = BLE_SCANNER_DEFAULT_CONFIG();
static bool s_inicializado = false;
static bool s_escaneo_activo = false;
static bool s_host_sincronizado = false;
static QueueHandle_t s_detection_queue = NULL;

// Control térmico optimizado para ESP32-S3-MINI-1
static bool s_control_termico_activo = true;
static float s_temp_eco = BLE_SCANNER_TEMP_ECO;
static float s_temp_warning = BLE_SCANNER_TEMP_WARNING;
static float s_temp_critical = BLE_SCANNER_TEMP_CRITICAL;
static float s_temp_emergency = BLE_SCANNER_TEMP_EMERGENCY;
static float s_temperatura_actual = 25.0f;
static ble_thermal_mode_t s_modo_termico = BLE_THERMAL_MODE_NORMAL;
static ble_presence_state_t s_estado_presencia = BLE_PRESENCE_UNKNOWN;
static temperature_sensor_handle_t s_temp_sensor = NULL;
static TaskHandle_t s_temp_task_handle = NULL;

// Parámetros de escaneo optimizados para diferentes modos térmicos
static struct ble_gap_disc_params s_scan_params[5] = {0}; // Para cada modo térmico

// Estadísticas y control de enfriamiento
static float s_temp_samples[60] = {0}; // 1 minuto de muestras
static uint8_t s_temp_sample_index = 0;
static float s_temp_maxima = 25.0f;
static uint32_t s_tiempo_critico_total = 0;
static bool s_enfriamiento_forzado = false;
static int64_t s_fin_enfriamiento_forzado = 0;
static uint32_t s_detecciones_globales = 0;

// Control de presencia para optimización
static int64_t s_ultima_deteccion_cualquiera = 0;
static int s_ciclos_consecutivos_sin_deteccion = 0;
static int s_ciclos_consecutivos_con_deteccion = 0;

// Prototipos
static void detection_task(void *param);
static void temp_monitor_task_s3(void *param);
static esp_err_t inicializar_sensor_temperatura_s3(void);
static float leer_temperatura_s3(void);
static void configurar_parametros_escaneo_s3(void);
static ble_thermal_mode_t determinar_modo_termico_s3(void);
static ble_presence_state_t determinar_estado_presencia_s3(void);
static void aplicar_modo_termico_s3(ble_thermal_mode_t nuevo_modo);
static esp_err_t iniciar_escaneo_con_modo_s3(ble_thermal_mode_t modo);
static esp_err_t iniciar_escaneo_s3(void);
static void crear_mac_hash(uint8_t target_idx);

/**
 * Hash para comparación rápida de MACs
 */
static void crear_mac_hash(uint8_t target_idx)
{
    const uint8_t *mac = s_targets[target_idx].mac;
    s_targets[target_idx].mac_hash = ((uint32_t)mac[0] << 24) |
                                     ((uint32_t)mac[1] << 16) |
                                     ((uint32_t)mac[2] << 8) |
                                     mac[3];
}

/**
 * Callback de escaneo BLE optimizado para ESP32-S3-MINI-1
 */
static IRAM_ATTR int ble_app_scan_cb_s3(struct ble_gap_event *event, void *arg)
{
    if (event->type == BLE_GAP_EVENT_DISC) {
        const uint8_t *adv_mac = event->disc.addr.val;
        int64_t now = esp_timer_get_time() / 1000;

        // Hash rápido para filtrado inicial
        uint32_t adv_hash = ((uint32_t)adv_mac[0] << 24) |
                            ((uint32_t)adv_mac[1] << 16) |
                            ((uint32_t)adv_mac[2] << 8) |
                            adv_mac[3];

        // Comparar con MACs objetivo
        for (int target_idx = 0; target_idx < BLE_SCANNER_MAX_TARGET_DEVICES; target_idx++) {
            if (!s_targets[target_idx].en_uso || adv_hash != s_targets[target_idx].mac_hash) {
                continue;
            }

            // Verificación completa de MAC
            bool match = true;
            for (int i = 0; i < 6; i++) {
                if (adv_mac[i] != s_targets[target_idx].mac[i]) {
                    match = false;
                    break;
                }
            }

            if (match) {
                // Actualizar estadísticas desde ISR
                portENTER_CRITICAL(&s_ble_mux);
                s_targets[target_idx].detectado = true;
                s_targets[target_idx].detecciones_totales++;
                s_targets[target_idx].ultima_deteccion = now;
                s_detecciones_globales++;
                portEXIT_CRITICAL(&s_ble_mux);

                // Enviar a cola para procesamiento
                if (s_detection_queue != NULL) {
                    detection_info_t info = {
                        .target_idx = target_idx,
                        .rssi = event->disc.rssi,
                        .timestamp = now
                    };
                    xQueueSendFromISR(s_detection_queue, &info, NULL);
                }
                break;
            }
        }
    }
    return 0;
}

/**
 * Tarea para procesar detecciones con estadísticas
 */
static void detection_task(void *param)
{
    detection_info_t info;
    static bool primera_deteccion[BLE_SCANNER_MAX_TARGET_DEVICES] = {0};

    while (1) {
        if (xQueueReceive(s_detection_queue, &info, portMAX_DELAY)) {
            // Log solo primera detección para reducir spam
            if (!primera_deteccion[info.target_idx]) {
                primera_deteccion[info.target_idx] = true;
                ESP_LOGI(TAG, "🎯 Dispositivo #%d detectado! RSSI:%d", info.target_idx, info.rssi);
                
                // Reporte MQTT de primera detección
                const char *mac_clean = sta_wifi_get_mac_clean();
                char topic[80];
                snprintf(topic, sizeof(topic), "dispositivos/%s/deteccion", mac_clean);
                char json[120];
                snprintf(json, sizeof(json), 
                        "{\"dispositivo\":%d,\"rssi\":%d,\"primera_vez\":true,\"timestamp\":%lld}", 
                        info.target_idx, info.rssi, info.timestamp);
                mqtt_service_enviar_dato(topic, json, 1, 0);
            }
            
            // Actualizar timestamp global
            s_ultima_deteccion_cualquiera = info.timestamp;
        }
    }
}

/**
 * Inicializar sensor de temperatura específico para ESP32-S3-MINI-1
 */
static esp_err_t inicializar_sensor_temperatura_s3(void)
{
    ESP_LOGI(TAG, "🌡️ Inicializando sensor térmico ESP32-S3-MINI-1...");

    temperature_sensor_config_t temp_config = {
        .range_min = 20,    // Rango optimizado para operación normal
        .range_max = 90,    // Hasta 90°C para ESP32-S3
        .clk_src = TEMPERATURE_SENSOR_CLK_SRC_DEFAULT
    };

    ESP_RETURN_ON_ERROR(temperature_sensor_install(&temp_config, &s_temp_sensor),
                       TAG, "Error instalando sensor térmico");
    ESP_RETURN_ON_ERROR(temperature_sensor_enable(s_temp_sensor),
                       TAG, "Error habilitando sensor térmico");

    ESP_LOGI(TAG, "✅ Sensor térmico ESP32-S3 inicializado correctamente");
    return ESP_OK;
}

/**
 * Leer temperatura con filtrado para ESP32-S3-MINI-1
 */
static float leer_temperatura_s3(void)
{
    static float temp_filtrada = 25.0f;
    float temp_nueva = 25.0f;
    
    if (s_temp_sensor != NULL) {
        if (temperature_sensor_get_celsius(s_temp_sensor, &temp_nueva) == ESP_OK) {
            // Filtro simple para suavizar lecturas (ESP32-S3 puede tener variaciones)
            temp_filtrada = (temp_filtrada * 0.8f) + (temp_nueva * 0.2f);
        }
    }
    
    return temp_filtrada;
}

/**
 * Configurar parámetros de escaneo optimizados para AUSENCIA prolongada
 */
static void configurar_parametros_escaneo_s3(void)
{
    // NORMAL: Máxima detección cuando temperatura es buena
    s_scan_params[BLE_THERMAL_MODE_NORMAL].passive = s_config.passive;
    s_scan_params[BLE_THERMAL_MODE_NORMAL].itvl = 0x0008;      // 5ms intervalo (MÁS AGRESIVO)
    s_scan_params[BLE_THERMAL_MODE_NORMAL].window = 0x0008;    // 5ms ventana (100%)
    s_scan_params[BLE_THERMAL_MODE_NORMAL].filter_duplicates = false; // No filtrar para máxima detección
    
    // ECO: Primer nivel de enfriamiento pero manteniendo buena detección
    s_scan_params[BLE_THERMAL_MODE_ECO].passive = true;
    s_scan_params[BLE_THERMAL_MODE_ECO].itvl = 0x0010;         // 10ms intervalo
    s_scan_params[BLE_THERMAL_MODE_ECO].window = 0x0008;       // 5ms ventana (50%)
    s_scan_params[BLE_THERMAL_MODE_ECO].filter_duplicates = false;
    
    // WARNING: Reducción moderada pero funcional para ausencia
    s_scan_params[BLE_THERMAL_MODE_WARNING].passive = true;
    s_scan_params[BLE_THERMAL_MODE_WARNING].itvl = 0x0020;     // 20ms intervalo
    s_scan_params[BLE_THERMAL_MODE_WARNING].window = 0x0008;   // 5ms ventana (25%)
    s_scan_params[BLE_THERMAL_MODE_WARNING].filter_duplicates = true;
    
    // CRITICAL: Reducción agresiva pero manteniendo detección básica
    s_scan_params[BLE_THERMAL_MODE_CRITICAL].passive = true;
    s_scan_params[BLE_THERMAL_MODE_CRITICAL].itvl = 0x0080;    // 80ms intervalo
    s_scan_params[BLE_THERMAL_MODE_CRITICAL].window = 0x0008;  // 5ms ventana (6.25%)
    s_scan_params[BLE_THERMAL_MODE_CRITICAL].filter_duplicates = true;
    
    // EMERGENCY: Supervivencia mínima pero seguir funcionando
    s_scan_params[BLE_THERMAL_MODE_EMERGENCY].passive = true;
    s_scan_params[BLE_THERMAL_MODE_EMERGENCY].itvl = 0x0280;   // 400ms intervalo
    s_scan_params[BLE_THERMAL_MODE_EMERGENCY].window = 0x0004; // 2.5ms ventana (0.625%)
    s_scan_params[BLE_THERMAL_MODE_EMERGENCY].filter_duplicates = true;
}

/**
 * Determinar estado de presencia con lógica optimizada
 */
static ble_presence_state_t determinar_estado_presencia_s3(void)
{
    int64_t now = esp_timer_get_time() / 1000;
    int64_t tiempo_sin_deteccion = now - s_ultima_deteccion_cualquiera;
    
    // Verificar detecciones recientes
    bool hay_detecciones_recientes = false;
    portENTER_CRITICAL(&s_ble_mux);
    for (int i = 0; i < BLE_SCANNER_MAX_TARGET_DEVICES; i++) {
        if (s_targets[i].en_uso && s_targets[i].detectado) {
            hay_detecciones_recientes = true;
            break;
        }
    }
    portEXIT_CRITICAL(&s_ble_mux);
    
    if (hay_detecciones_recientes) {
        s_ciclos_consecutivos_con_deteccion++;
        s_ciclos_consecutivos_sin_deteccion = 0;
        
        // Confirmar presencia después de algunas detecciones
        if (s_ciclos_consecutivos_con_deteccion >= 3) {
            return BLE_PRESENCE_PRESENT;
        } else {
            return BLE_PRESENCE_TRANSITIONING;
        }
    } else {
        s_ciclos_consecutivos_sin_deteccion++;
        s_ciclos_consecutivos_con_deteccion = 0;
        
        // Confirmar ausencia
        if (tiempo_sin_deteccion > 45000) { // 45 segundos
            if (s_ciclos_consecutivos_sin_deteccion >= 15) { // ~3 segundos sin detección
                return BLE_PRESENCE_ABSENT;
            } else {
                return BLE_PRESENCE_TRANSITIONING;
            }
        }
    }
    
    return s_estado_presencia; // Mantener estado actual
}

/**
 * Determinar modo térmico optimizado para trabajo AUSENTE intensivo
 */
static ble_thermal_mode_t determinar_modo_termico_s3(void)
{
    // Si hay enfriamiento forzado
    if (s_enfriamiento_forzado) {
        return BLE_THERMAL_MODE_ECO;
    }
    
    // **OPTIMIZACIÓN MEJORADA: Control más granular para cuadros eléctricos**
    // Considerar la velocidad de subida de temperatura
    static float temp_previa = 25.0f;
    float velocidad_subida = s_temperatura_actual - temp_previa;
    temp_previa = s_temperatura_actual;
    
    // Si sube muy rápido, ser más agresivo preventivamente
    bool subida_rapida = velocidad_subida > 1.0f; // >1°C por ciclo de monitoreo
    
    if (s_temperatura_actual >= s_temp_emergency) {        // 85°C+
        return BLE_THERMAL_MODE_EMERGENCY;  // 0.625% duty cycle
    } else if (s_temperatura_actual >= s_temp_critical) {  // 75°C+
        return BLE_THERMAL_MODE_CRITICAL;   // 6.25% duty cycle
    } else if (s_temperatura_actual >= s_temp_warning) {   // 65°C+
        return BLE_THERMAL_MODE_WARNING;    // 25% duty cycle
    } else if (s_temperatura_actual >= s_temp_eco || subida_rapida) { // 55°C+ O subida rápida
        return BLE_THERMAL_MODE_ECO;        // 50% duty cycle
    } else {
        return BLE_THERMAL_MODE_NORMAL;     // 100% duty cycle
    }
}

/**
 * Tarea de monitoreo térmico optimizada para trabajo AUSENTE intensivo
 */
static void temp_monitor_task_s3(void *param)
{
    ESP_LOGI(TAG, "🔥 Monitor térmico ESP32-S3-MINI-1 iniciado (MODO AUSENTE OPTIMIZADO)");
    ESP_LOGI(TAG, "📊 Umbrales AUSENTE: ECO=%.1f°C, WARNING=%.1f°C, CRITICAL=%.1f°C, EMERGENCY=%.1f°C",
             s_temp_eco, s_temp_warning, s_temp_critical, s_temp_emergency);

    TickType_t xLastWakeTime = xTaskGetTickCount();
    float last_reported_temp = -100.0f;
    TickType_t last_mqtt_report = 0;
    const TickType_t mqtt_interval = pdMS_TO_TICKS(90000); // 1.5 minutos
    
    // **MEJORA: Contador de tiempo en emergencia para estadísticas**
    uint32_t tiempo_total_emergencia = 0;
    bool estaba_en_emergencia = false;
    
    configurar_parametros_escaneo_s3();

    // Topic MQTT
    char temp_topic[80];
    const char *mac_clean = sta_wifi_get_mac_clean();
    snprintf(temp_topic, sizeof(temp_topic), "dispositivos/%s/termico_ausente", mac_clean);

    while (1) {
        uint32_t intervalo_actual = s_config.intervalo_monitoreo_ms;
        
        // Leer y filtrar temperatura
        s_temperatura_actual = leer_temperatura_s3();
        
        // **MEJORA: Detectar condiciones críticas de cuadro eléctrico**
        bool condicion_critica = false;
        if (s_temperatura_actual >= s_temp_emergency) {
            condicion_critica = true;
            if (!estaba_en_emergencia) {
                ESP_LOGW(TAG, "🚨 CONDICIÓN TÉRMICA CRÍTICA: %.1f°C - Cuadro eléctrico sobrecalentado", s_temperatura_actual);
                estaba_en_emergencia = true;
            }
            tiempo_total_emergencia += intervalo_actual;
        } else {
            estaba_en_emergencia = false;
        }
        
        // Actualizar estadísticas
        s_temp_samples[s_temp_sample_index] = s_temperatura_actual;
        s_temp_sample_index = (s_temp_sample_index + 1) % 60;
        if (s_temperatura_actual > s_temp_maxima) {
            s_temp_maxima = s_temperatura_actual;
        }
        
        // Verificar enfriamiento forzado
        if (s_enfriamiento_forzado && (esp_timer_get_time() / 1000) > s_fin_enfriamiento_forzado) {
            s_enfriamiento_forzado = false;
            ESP_LOGI(TAG, "❄️ Enfriamiento forzado completado");
        }
        
        // Determinar y aplicar modo térmico
        ble_thermal_mode_t nuevo_modo = determinar_modo_termico_s3();
        if (s_control_termico_activo && nuevo_modo != s_modo_termico) {
            aplicar_modo_termico_s3(nuevo_modo);
        }
        
        // Contar tiempo en modo crítico
        if (s_modo_termico >= BLE_THERMAL_MODE_CRITICAL) {
            s_tiempo_critico_total += intervalo_actual / 1000;
        }
        
        // **ENFRIAMIENTO PREDICTIVO MEJORADO**
        static float temp_anterior = 25.0f;
        static int ciclos_calentandose = 0;
        
        float delta_temp = s_temperatura_actual - temp_anterior;
        if (delta_temp > 1.5f) { // Subida sostenida
            ciclos_calentandose++;
            if (ciclos_calentandose >= 3 && s_temperatura_actual > s_temp_eco - 8.0f) {
                ESP_LOGW(TAG, "🌡️ Calentamiento sostenido detectado (%d ciclos, %.1f°C), enfriamiento preventivo", 
                         ciclos_calentandose, delta_temp);
                ble_scanner_forzar_enfriamiento(15000); // 15 segundos
                ciclos_calentandose = 0;
            }
        } else {
            ciclos_calentandose = 0;
        }
        temp_anterior = s_temperatura_actual;
        
        // Reporte MQTT mejorado
        TickType_t now_tick = xTaskGetTickCount();
        bool cambio_significativo = fabsf(s_temperatura_actual - last_reported_temp) >= 2.0f;
        bool es_momento_reporte = (now_tick - last_mqtt_report) >= mqtt_interval;
        bool reportar_emergencia = condicion_critica && ((now_tick / pdMS_TO_TICKS(30000)) % 1 == 0); // Cada 30s en emergencia
        
        if (cambio_significativo || es_momento_reporte || reportar_emergencia) {
            const char* modos[] = {"NORMAL", "ECO", "WARNING", "CRITICAL", "EMERGENCY"};
            
            // Calcular duty cycle actual
            struct ble_gap_disc_params *params = &s_scan_params[s_modo_termico];
            float duty_actual = (float)(params->window * 100) / params->itvl;
            
            // Determinar estado del cuadro eléctrico
            const char* estado_cuadro = "NORMAL";
            if (s_temperatura_actual >= s_temp_emergency) {
                estado_cuadro = "SOBRECALENTADO";
            } else if (s_temperatura_actual >= s_temp_critical) {
                estado_cuadro = "CALIENTE";
            } else if (s_temperatura_actual >= s_temp_warning) {
                estado_cuadro = "TIBIO";
            }
            
            char json[350];
            snprintf(json, sizeof(json),
                    "{\"temp\":%.1f,\"modo_termico\":\"%s\",\"duty_cycle\":\"%.1f%%\",\"temp_max\":%.1f,"
                    "\"detecciones\":%lu,\"tiempo_critico\":%lu,\"tiempo_emergencia\":%lu,"
                    "\"trabajo\":\"INTENSIVO_AUSENTE\",\"estado_cuadro\":\"%s\",\"intervalo_escaneo\":%dms}",
                    s_temperatura_actual, modos[s_modo_termico], duty_actual, s_temp_maxima,
                    s_detecciones_globales, s_tiempo_critico_total, tiempo_total_emergencia / 1000,
                    estado_cuadro, (params->itvl * 625) / 1000);
            
            mqtt_service_enviar_dato(temp_topic, json, 1, 0);
            last_reported_temp = s_temperatura_actual;
            last_mqtt_report = now_tick;
        }
        
        // **INTERVALO ADAPTATIVO MEJORADO PARA CUADROS ELÉCTRICOS**
        if (condicion_critica) {
            intervalo_actual = 25;   // 25ms - monitoreo ultra-crítico
        } else if (s_temperatura_actual >= s_temp_critical) {
            intervalo_actual = 50;   // 50ms - monitoreo crítico
        } else if (s_temperatura_actual >= s_temp_warning) {
            intervalo_actual = 100;  // 100ms - monitoreo frecuente
        } else if (s_temperatura_actual >= s_temp_eco) {
            intervalo_actual = 150;  // 150ms - monitoreo normal
        } else {
            intervalo_actual = 200;  // 200ms - monitoreo relajado
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(intervalo_actual));
    }
}

/**
 * Aplicar cambios según modo térmico (optimizado para ausencia)
 */
static void aplicar_modo_termico_s3(ble_thermal_mode_t nuevo_modo)
{
    if (!s_inicializado || !s_control_termico_activo) {
        return;
    }
    
    const char* modos[] = {"NORMAL", "ECO", "WARNING", "CRITICAL", "EMERGENCY"};
    
    // Calcular duty cycle real
    struct ble_gap_disc_params *params = &s_scan_params[nuevo_modo];
    float duty = (float)(params->window * 100) / params->itvl;
    
    ESP_LOGI(TAG, "🌡️ Modo térmico AUSENTE: %s -> %s (%.1f°C) Duty: %.1f%% Int: %dms",
             modos[s_modo_termico], modos[nuevo_modo], s_temperatura_actual, 
             duty, (params->itvl * 625) / 1000);
    
    s_modo_termico = nuevo_modo;
    
    // Reiniciar escaneo con nuevos parámetros si está activo
    if (s_escaneo_activo) {
        ble_scanner_detener();
        // En modo ausente, SIEMPRE mantener escaneo activo, incluso en emergencia
        // porque estado_automatico depende de las detecciones
        iniciar_escaneo_con_modo_s3(nuevo_modo);
        
        // Log adicional para debugging
        if (nuevo_modo >= BLE_THERMAL_MODE_CRITICAL) {
            ESP_LOGW(TAG, "⚠️ BLE en modo térmico crítico pero manteniendo escaneo para detección");
        }
    }
}

/**
 * Iniciar escaneo con modo térmico específico
 */
static esp_err_t iniciar_escaneo_con_modo_s3(ble_thermal_mode_t modo)
{
    if (!s_inicializado) {
        return ESP_ERR_INVALID_STATE;
    }
    
    struct ble_gap_disc_params *params = &s_scan_params[modo];
    
    int duration_ticks = BLE_HS_FOREVER;
    if (s_config.duration_ms > 0) {
        duration_ticks = s_config.duration_ms / 10;
    }
    
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, duration_ticks, params, ble_app_scan_cb_s3, NULL);
    
    if (rc != 0) {
        ESP_LOGE(TAG, "❌ Error iniciando escaneo modo %d: %d", modo, rc);
        return ESP_FAIL;
    }
    
    s_escaneo_activo = true;
    float duty = (float)(params->window * 100) / params->itvl;
    ESP_LOGI(TAG, "🔄 Escaneo iniciado: Modo %d, Duty %.1f%%, Int %dms", 
             modo, duty, (params->itvl * 625) / 1000);
    
    return ESP_OK;
}

/**
 * Iniciar escaneo con modo térmico actual
 */
static esp_err_t iniciar_escaneo_s3(void)
{
    return iniciar_escaneo_con_modo_s3(s_modo_termico);
}

// Callback sincronización host
static void on_ble_host_sync(void)
{
    ESP_LOGI(TAG, "🔗 Host BLE sincronizado");
    s_host_sincronizado = true;
    
    if (s_inicializado) {
        esp_err_t ret = iniciar_escaneo_s3();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Error iniciando escaneo automático: %s", esp_err_to_name(ret));
        }
    }
}

// Tarea host BLE
static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "🚀 Tarea host BLE iniciada");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// Implementación de funciones públicas
esp_err_t ble_scanner_iniciar(const ble_scanner_config_t *config)
{
    if (s_inicializado) {
        ESP_LOGW(TAG, "⚠️ Scanner ya inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    // Aplicar configuración
    if (config != NULL) {
        memcpy(&s_config, config, sizeof(ble_scanner_config_t));
        s_control_termico_activo = config->control_termico_activo;
        s_temp_eco = config->temp_eco;
        s_temp_warning = config->temp_warning;
        s_temp_critical = config->temp_critical;
        s_temp_emergency = config->temp_emergency;
    }

    ESP_LOGI(TAG, "🚀 Inicializando BLE Scanner para ESP32-S3-MINI-1...");

    // Inicializar sensor de temperatura
    esp_err_t ret = inicializar_sensor_temperatura_s3();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠️ Sensor térmico no disponible, control térmico desactivado");
        s_control_termico_activo = false;
    }

    // Crear cola de detecciones
    s_detection_queue = xQueueCreate(20, sizeof(detection_info_t));
    if (!s_detection_queue) {
        ESP_LOGE(TAG, "❌ Error creando cola de detecciones");
        return ESP_ERR_NO_MEM;
    }

    // Crear tarea de procesamiento de detecciones
    BaseType_t res = xTaskCreate(detection_task, "ble_detect_s3", 4096, NULL, 6, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "❌ Error creando tarea de detección");
        vQueueDelete(s_detection_queue);
        return ESP_FAIL;
    }

    // Crear tarea de monitoreo térmico
    if (s_control_termico_activo) {
        res = xTaskCreate(temp_monitor_task_s3, "temp_monitor_s3", 4096, NULL, 5, &s_temp_task_handle);
        if (res != pdPASS) {
            ESP_LOGE(TAG, "❌ Error creando tarea de monitoreo térmico");
        } else {
            ESP_LOGI(TAG, "🌡️ Monitor térmico creado (intervalo: %lums)", s_config.intervalo_monitoreo_ms);
        }
    }

    // Configurar callback y inicializar NimBLE
    ble_hs_cfg.sync_cb = on_ble_host_sync;
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error inicializando NimBLE: %d", ret);
        return ESP_FAIL;
    }

    nimble_port_freertos_init(ble_host_task);
    s_inicializado = true;

    ESP_LOGI(TAG, "✅ BLE Scanner ESP32-S3-MINI-1 inicializado correctamente");
    ESP_LOGI(TAG, "📊 Control térmico: %s | Detección rápida ausente: %s",
             s_control_termico_activo ? "ON" : "OFF",
             s_config.deteccion_rapida_ausente ? "ON" : "OFF");

    return ESP_OK;
}

// ...existing code... (resto de funciones con nombres originales pero optimizadas)

esp_err_t ble_scanner_deinicializar(void)
{
    if (!s_inicializado) {
        return ESP_ERR_INVALID_STATE;
    }

    ble_scanner_detener();

    if (s_temp_task_handle) {
        vTaskDelete(s_temp_task_handle);
        s_temp_task_handle = NULL;
    }
    
    if (s_temp_sensor) {
        temperature_sensor_disable(s_temp_sensor);
        temperature_sensor_uninstall(s_temp_sensor);
        s_temp_sensor = NULL;
    }

    if (s_detection_queue) {
        vQueueDelete(s_detection_queue);
        s_detection_queue = NULL;
    }

    nimble_port_stop();
    nimble_port_deinit();
    s_inicializado = false;

    ESP_LOGI(TAG, "🛑 BLE Scanner deinicializado");
    return ESP_OK;
}

esp_err_t ble_scanner_detener(void)
{
    if (!s_inicializado || !s_escaneo_activo) {
        return ESP_OK;
    }

    int rc = ble_gap_disc_cancel();
    if (rc != 0) {
        ESP_LOGE(TAG, "❌ Error deteniendo escaneo: %d", rc);
        return ESP_FAIL;
    }

    s_escaneo_activo = false;
    ESP_LOGI(TAG, "⏸️ Escaneo BLE detenido");
    return ESP_OK;
}

esp_err_t ble_scanner_reiniciar(void)
{
    esp_err_t ret = ble_scanner_detener();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }
    return iniciar_escaneo_s3();
}

bool ble_scanner_tag_detectado(uint8_t mac_index)
{
    if (mac_index >= BLE_SCANNER_MAX_TARGET_DEVICES || !s_targets[mac_index].en_uso) {
        return false;
    }

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
    for (int i = 0; i < BLE_SCANNER_MAX_TARGET_DEVICES; i++) {
        if (s_targets[i].en_uso && s_targets[i].detectado) {
            detectado = true;
            break;
        }
    }
    portEXIT_CRITICAL(&s_ble_mux);
    return detectado;
}

esp_err_t ble_scanner_configurar_mac_objetivo(uint8_t mac_index, const uint8_t *mac)
{
    if (mac_index >= BLE_SCANNER_MAX_TARGET_DEVICES || !mac) {
        return ESP_ERR_INVALID_ARG;
    }

    // Almacenar MAC en orden invertido
    for (int i = 0; i < 6; i++) {
        s_targets[mac_index].mac[i] = mac[5 - i];
    }

    portENTER_CRITICAL(&s_ble_mux);
    s_targets[mac_index].en_uso = true;
    s_targets[mac_index].detectado = false;
    s_targets[mac_index].detecciones_totales = 0;
    s_targets[mac_index].ultima_deteccion = 0;
    portEXIT_CRITICAL(&s_ble_mux);

    crear_mac_hash(mac_index);

    ESP_LOGI(TAG, "🎯 MAC #%d configurada: %02X:%02X:%02X:%02X:%02X:%02X",
             mac_index, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return ESP_OK;
}

esp_err_t ble_scanner_configurar_mac_objetivo_texto(uint8_t mac_index, const char *mac_str)
{
    if (mac_index >= BLE_SCANNER_MAX_TARGET_DEVICES || !mac_str) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t mac[6] = {0};
    size_t len = strlen(mac_str);

    if (len == 12) {
        // Formato sin separadores
        for (int i = 0; i < 6; i++) {
            char byte_str[3] = {mac_str[i * 2], mac_str[i * 2 + 1], 0};
            mac[i] = (uint8_t)strtol(byte_str, NULL, 16);
        }
    } else if (len == 17) {
        // Formato con separadores
        for (int i = 0; i < 6; i++) {
            char byte_str[3] = {mac_str[i * 3], mac_str[i * 3 + 1], 0};
            mac[i] = (uint8_t)strtol(byte_str, NULL, 16);
        }
    } else {
        ESP_LOGE(TAG, "❌ Formato MAC inválido: %s", mac_str);
        return ESP_ERR_INVALID_ARG;
    }

    return ble_scanner_configurar_mac_objetivo(mac_index, mac);
}

esp_err_t ble_scanner_limpiar_macs_objetivo(void)
{
    portENTER_CRITICAL(&s_ble_mux);
    memset(s_targets, 0, sizeof(s_targets));
    portEXIT_CRITICAL(&s_ble_mux);

    ESP_LOGI(TAG, "🧹 MACs objetivo limpiadas");
    return ESP_OK;
}

bool ble_scanner_esta_activo(void)
{
    return s_escaneo_activo;
}

float ble_scanner_obtener_temperatura(void)
{
    return s_temperatura_actual;
}

ble_thermal_mode_t ble_scanner_obtener_modo_termico(void)
{
    return s_modo_termico;
}

ble_presence_state_t ble_scanner_obtener_estado_presencia(void)
{
    return s_estado_presencia;
}

esp_err_t ble_scanner_configurar_umbrales_temperatura(float temp_eco, float temp_warning, 
                                                     float temp_critical, float temp_emergency)
{
    if (temp_eco >= temp_warning || temp_warning >= temp_critical || 
        temp_critical >= temp_emergency || temp_eco < 30.0f || temp_emergency > 90.0f) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_temp_eco = temp_eco;
    s_temp_warning = temp_warning;
    s_temp_critical = temp_critical;
    s_temp_emergency = temp_emergency;
    
    ESP_LOGI(TAG, "🌡️ Umbrales actualizados: ECO=%.1f°C, WARN=%.1f°C, CRIT=%.1f°C, EMER=%.1f°C",
             s_temp_eco, s_temp_warning, s_temp_critical, s_temp_emergency);
    
    return ESP_OK;
}

esp_err_t ble_scanner_activar_control_termico(bool activar)
{
    s_control_termico_activo = activar;
    ESP_LOGI(TAG, "🌡️ Control térmico %s", activar ? "ACTIVADO" : "DESACTIVADO");
    return ESP_OK;
}

esp_err_t ble_scanner_forzar_enfriamiento(uint32_t duracion_ms)
{
    if (!s_inicializado) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_enfriamiento_forzado = true;
    s_fin_enfriamiento_forzado = (esp_timer_get_time() / 1000) + duracion_ms;
    
    ESP_LOGI(TAG, "❄️ Enfriamiento forzado para trabajo ausente: %lums", duracion_ms);
    
    // Aplicar inmediatamente modo ECO si está escaneando
    if (s_escaneo_activo && s_modo_termico != BLE_THERMAL_MODE_ECO) {
        aplicar_modo_termico_s3(BLE_THERMAL_MODE_ECO);
    }
    
    return ESP_OK;
}

esp_err_t ble_scanner_obtener_estadisticas(float *temp_promedio, float *temp_maxima, 
                                          uint32_t *detecciones_totales, uint32_t *tiempo_critico_seg)
{
    if (!temp_promedio || !temp_maxima || !detecciones_totales || !tiempo_critico_seg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Calcular promedio
    float suma = 0;
    int muestras = 0;
    for (int i = 0; i < 60; i++) {
        if (s_temp_samples[i] > 0) {
            suma += s_temp_samples[i];
            muestras++;
        }
    }
    
    *temp_promedio = (muestras > 0) ? suma / muestras : s_temperatura_actual;
    *temp_maxima = s_temp_maxima;
    *detecciones_totales = s_detecciones_globales;
    *tiempo_critico_seg = s_tiempo_critico_total;
    
    return ESP_OK;
}
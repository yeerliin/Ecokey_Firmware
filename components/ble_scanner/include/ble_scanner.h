#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Número máximo de dispositivos MAC objetivo que se pueden monitorear
 */
#define BLE_SCANNER_MAX_TARGET_DEVICES 10

/**
 * @brief Umbrales de temperatura optimizados para ESP32-S3-MINI-1 en trabajo AUSENTE intensivo
 * Estos valores están calibrados específicamente para cuando el BLE trabaja constantemente
 * buscando dispositivos ausentes (escenario más demandante térmicamente)
 */
#define BLE_SCANNER_TEMP_ECO           55  /**< Inicio enfriamiento preventivo - trabajo intensivo */
#define BLE_SCANNER_TEMP_WARNING       65  /**< Reducir duty cycle a 25% - calor moderado */
#define BLE_SCANNER_TEMP_CRITICAL      75  /**< Reducir duty cycle a 6.25% - calor alto */
#define BLE_SCANNER_TEMP_EMERGENCY     85  /**< Reducir duty cycle a 0.625% - supervivencia */

/**
 * @brief Modos de operación térmica optimizados para trabajo AUSENTE intensivo
 * Nota: estado_automatico maneja el ciclo presente/ausente, este módulo se optimiza
 * para el escenario más demandante: búsqueda continua cuando está ausente
 */
typedef enum {
    BLE_THERMAL_MODE_NORMAL = 0,      /**< 100% duty - 5ms/5ms - detección máxima */
    BLE_THERMAL_MODE_ECO,             /**< 50% duty - 5ms/10ms - enfriamiento suave */
    BLE_THERMAL_MODE_WARNING,         /**< 25% duty - 5ms/20ms - reducción moderada */
    BLE_THERMAL_MODE_CRITICAL,        /**< 6.25% duty - 5ms/80ms - reducción agresiva */
    BLE_THERMAL_MODE_EMERGENCY        /**< 0.625% duty - 2.5ms/400ms - supervivencia */
} ble_thermal_mode_t;

/**
 * @brief Estados de presencia para optimización específica
 */
typedef enum {
    BLE_PRESENCE_UNKNOWN = 0,         /**< Estado inicial */
    BLE_PRESENCE_ABSENT,              /**< Ausente - trabajo intenso BLE */
    BLE_PRESENCE_PRESENT,             /**< Presente - trabajo mínimo BLE */
    BLE_PRESENCE_TRANSITIONING        /**< En transición */
} ble_presence_state_t;

/**
 * @brief Configuración optimizada para trabajo AUSENTE intensivo en ESP32-S3-MINI-1
 */
typedef struct {
    bool passive;                     /**< Escaneo pasivo (recomendado para eficiencia térmica) */
    uint16_t scan_interval;           /**< Intervalo base (0.625ms units) - se adaptará según temperatura */
    uint16_t scan_window;             /**< Ventana base (0.625ms units) - se adaptará según temperatura */
    bool filter_duplicates;           /**< Filtrar duplicados - se desactiva en modo normal para máxima detección */
    int duration_ms;                  /**< Duración (0 = infinito) */
    bool control_termico_activo;      /**< Control térmico automático para trabajo intensivo */
    float temp_eco;                   /**< Umbral ECO personalizado */
    float temp_warning;               /**< Umbral WARNING personalizado */
    float temp_critical;              /**< Umbral CRITICAL personalizado */
    float temp_emergency;             /**< Umbral EMERGENCY personalizado */
    bool deteccion_rapida_ausente;    /**< OBSOLETO: siempre optimizado para ausencia */
    uint32_t intervalo_monitoreo_ms;  /**< Intervalo base de monitoreo (se adapta según temperatura) */
} ble_scanner_config_t;

/**
 * @brief Configuración por defecto optimizada para trabajo AUSENTE intensivo
 */
#define BLE_SCANNER_DEFAULT_CONFIG() { \
    .passive = true, \
    .scan_interval = 0x0008, \
    .scan_window = 0x0008, \
    .filter_duplicates = false, \
    .duration_ms = 0, \
    .control_termico_activo = true, \
    .temp_eco = BLE_SCANNER_TEMP_ECO, \
    .temp_warning = BLE_SCANNER_TEMP_WARNING, \
    .temp_critical = BLE_SCANNER_TEMP_CRITICAL, \
    .temp_emergency = BLE_SCANNER_TEMP_EMERGENCY, \
    .deteccion_rapida_ausente = true, \
    .intervalo_monitoreo_ms = 150 \
}

/**
 * @brief Inicializa el BLE scanner optimizado para ESP32-S3-MINI-1
 */
esp_err_t ble_scanner_iniciar(const ble_scanner_config_t* config);

/**
 * @brief Deinicializa el escáner BLE
 */
esp_err_t ble_scanner_deinicializar(void);

/**
 * @brief Detiene el escaneo BLE
 */
esp_err_t ble_scanner_detener(void);

/**
 * @brief Reinicia el escaneo BLE con parámetros térmicos actuales
 */
esp_err_t ble_scanner_reiniciar(void);

/**
 * @brief Consulta si el tag objetivo fue detectado
 */
bool ble_scanner_tag_detectado(uint8_t mac_index);

/**
 * @brief Consulta si cualquier tag objetivo fue detectado
 */
bool ble_scanner_cualquier_tag_detectado(void);

/**
 * @brief Define la dirección MAC objetivo en formato binario
 */
esp_err_t ble_scanner_configurar_mac_objetivo(uint8_t mac_index, const uint8_t *mac);

/**
 * @brief Define la dirección MAC objetivo en formato texto
 */
esp_err_t ble_scanner_configurar_mac_objetivo_texto(uint8_t mac_index, const char *mac_str);

/**
 * @brief Limpia todas las MACs objetivo configuradas
 */
esp_err_t ble_scanner_limpiar_macs_objetivo(void);

/**
 * @brief Consulta si el escáner BLE está activo
 */
bool ble_scanner_esta_activo(void);

/**
 * @brief Obtiene la temperatura actual del chip
 */
float ble_scanner_obtener_temperatura(void);

/**
 * @brief Obtiene el modo térmico actual
 */
ble_thermal_mode_t ble_scanner_obtener_modo_termico(void);

/**
 * @brief Obtiene el estado de presencia actual
 */
ble_presence_state_t ble_scanner_obtener_estado_presencia(void);

/**
 * @brief Configura umbrales de temperatura personalizados
 */
esp_err_t ble_scanner_configurar_umbrales_temperatura(float temp_eco, float temp_warning, 
                                                     float temp_critical, float temp_emergency);

/**
 * @brief Activa/desactiva control térmico automático
 */
esp_err_t ble_scanner_activar_control_termico(bool activar);

/**
 * @brief Fuerza enfriamiento temporal con modo ECO
 * Útil cuando se detecta subida rápida de temperatura durante trabajo intensivo
 * 
 * @param duracion_ms Duración del enfriamiento forzado
 * @return esp_err_t ESP_OK si la operación fue exitosa
 */
esp_err_t ble_scanner_forzar_enfriamiento(uint32_t duracion_ms);

/**
 * @brief Obtiene estadísticas de rendimiento térmico
 */
esp_err_t ble_scanner_obtener_estadisticas(float *temp_promedio, float *temp_maxima, 
                                          uint32_t *detecciones_totales, uint32_t *tiempo_critico_seg);

#ifdef __cplusplus
}
#endif
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
 * @brief Umbrales de temperatura para el control térmico (en °C)
 */
#define BLE_SCANNER_TEMP_WARNING       60  /**< Umbral de advertencia */
#define BLE_SCANNER_TEMP_CRITICAL      70  /**< Umbral crítico */

/**
 * @brief Modos de operación térmica
 */
typedef enum {
    BLE_THERMAL_MODE_NORMAL = 0,   /**< Operación normal */
    BLE_THERMAL_MODE_WARNING,      /**< Modo de advertencia - actividad reducida */
    BLE_THERMAL_MODE_CRITICAL      /**< Modo crítico - escaneo detenido */
} ble_thermal_mode_t;

/**
 * @brief Configuración para el escáner BLE
 */
typedef struct {
    bool passive;              /**< Modo de escaneo pasivo (true) o activo (false) */
    uint16_t scan_interval;    /**< Intervalo de escaneo (unidades de 0.625ms) */
    uint16_t scan_window;      /**< Ventana de escaneo (unidades de 0.625ms) */
    bool filter_duplicates;    /**< Filtrar dispositivos duplicados */
    int duration_ms;           /**< Duración del escaneo en ms (0 = infinito) */
    bool control_termico;      /**< Activar control térmico automático */
    float temp_warning;        /**< Temperatura de advertencia personalizada (°C) */
    float temp_critical;       /**< Temperatura crítica personalizada (°C) */
} ble_scanner_config_t;

/**
 * @brief Configuración por defecto para el escáner BLE
 */
#define BLE_SCANNER_DEFAULT_CONFIG() { \
    .passive = true, \
    .scan_interval = 0x0010, \
    .scan_window = 0x0010, \
    .filter_duplicates = false, \
    .duration_ms = 0, \
    .control_termico = true, \
    .temp_warning = BLE_SCANNER_TEMP_WARNING, \
    .temp_critical = BLE_SCANNER_TEMP_CRITICAL \
}

/**
 * @brief Inicializa el BLE scanner.
 *
 * Inicializa el stack NimBLE, la memoria NVS y configura el escaneo BLE.
 * Esta función debe ser llamada una sola vez al inicio del programa.
 * 
 * Compatible con ESP-IDF v5.x y superiores.
 *
 * @param config Configuración para el escáner BLE, NULL para usar valores por defecto
 * @return ESP_OK si la inicialización fue exitosa, código de error en caso contrario
 */
esp_err_t ble_scanner_iniciar(const ble_scanner_config_t* config);

/**
 * @brief Deinicializa el escáner BLE y libera los recursos utilizados
 *
 * @return ESP_OK si la deinicialización fue exitosa, código de error en caso contrario
 */
esp_err_t ble_scanner_deinicializar(void);

/**
 * @brief Detiene el escaneo BLE
 *
 * @return ESP_OK si se detuvo correctamente, código de error en caso contrario
 */
esp_err_t ble_scanner_detener(void);

/**
 * @brief Reinicia el escaneo BLE con la configuración actual
 *
 * @return ESP_OK si se reinició correctamente, código de error en caso contrario
 */
esp_err_t ble_scanner_reiniciar(void);

/**
 * @brief Consulta si el tag objetivo fue detectado.
 *
 * @param mac_index Índice del dispositivo MAC objetivo (0 a BLE_SCANNER_MAX_TARGET_DEVICES-1)
 * @return true si el dispositivo con la MAC objetivo ha sido detectado
 *         desde la última consulta. El flag se reinicia después de la llamada.
 */
bool ble_scanner_tag_detectado(uint8_t mac_index);

/**
 * @brief Consulta si cualquier tag objetivo fue detectado.
 *
 * @return true si cualquier dispositivo con MAC objetivo ha sido detectado
 *         desde la última consulta. Los flags individuales se mantienen.
 */
bool ble_scanner_cualquier_tag_detectado(void);

/**
 * @brief Define la dirección MAC objetivo en formato de array binario.
 *
 * @param mac_index Índice del dispositivo MAC objetivo (0 a BLE_SCANNER_MAX_TARGET_DEVICES-1)
 * @param mac Puntero a array de 6 bytes que contiene la dirección MAC a buscar.
 * @return ESP_OK si la configuración fue exitosa, código de error en caso contrario
 */
esp_err_t ble_scanner_configurar_mac_objetivo(uint8_t mac_index, const uint8_t *mac);

/**
 * @brief Define la dirección MAC objetivo en formato de texto.
 *
 * Acepta formatos como "001B660F4AE2" o "00:1B:66:0F:4A:E2".
 *
 * @param mac_index Índice del dispositivo MAC objetivo (0 a BLE_SCANNER_MAX_TARGET_DEVICES-1)
 * @param mac_str Cadena de caracteres que representa la MAC.
 * @return ESP_OK si la conversión y configuración fueron exitosas, código de error en caso contrario
 */
esp_err_t ble_scanner_configurar_mac_objetivo_texto(uint8_t mac_index, const char *mac_str);

/**
 * @brief Limpia todas las MACs objetivo configuradas
 * 
 * @return ESP_OK si la operación fue exitosa
 */
esp_err_t ble_scanner_limpiar_macs_objetivo(void);

/**
 * @brief Consulta si el escáner BLE está actualmente en funcionamiento
 *
 * @return true si el escáner está activo, false en caso contrario
 */
bool ble_scanner_esta_activo(void);

/**
 * @brief Obtiene la temperatura actual del chip
 * 
 * @return float Temperatura en grados celsius
 */
float ble_scanner_obtener_temperatura(void);

/**
 * @brief Obtiene el modo térmico actual del escáner
 * 
 * @return ble_thermal_mode_t El modo térmico actual
 */
ble_thermal_mode_t ble_scanner_obtener_modo_termico(void);

/**
 * @brief Establece manualmente los umbrales de temperatura
 * 
 * @param temp_warning Temperatura de advertencia en °C
 * @param temp_critical Temperatura crítica en °C
 * @return esp_err_t ESP_OK si la operación fue exitosa
 */
esp_err_t ble_scanner_configurar_umbrales_temperatura(float temp_warning, float temp_critical);

/**
 * @brief Activa o desactiva el control térmico automático
 * 
 * @param activar true para activar, false para desactivar
 * @return esp_err_t ESP_OK si la operación fue exitosa
 */
esp_err_t ble_scanner_activar_control_termico(bool activar);

#ifdef __cplusplus
}
#endif
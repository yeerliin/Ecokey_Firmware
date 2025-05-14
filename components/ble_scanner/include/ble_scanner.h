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
 * @brief Configuración para el escáner BLE
 */
typedef struct {
    bool passive;              /**< Modo de escaneo pasivo (true) o activo (false) */
    uint16_t scan_interval;    /**< Intervalo de escaneo (unidades de 0.625ms) */
    uint16_t scan_window;      /**< Ventana de escaneo (unidades de 0.625ms) */
    bool filter_duplicates;    /**< Filtrar dispositivos duplicados */
    int duration_ms;           /**< Duración del escaneo en ms (0 = infinito) */
} ble_scanner_config_t;

/**
 * @brief Configuración por defecto para el escáner BLE
 */
#define BLE_SCANNER_DEFAULT_CONFIG() { \
    .passive = true, \
    .scan_interval = 0x0010, \
    .scan_window = 0x0010, \
    .filter_duplicates = false, \
    .duration_ms = 0 \
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


#ifdef __cplusplus
}
#endif
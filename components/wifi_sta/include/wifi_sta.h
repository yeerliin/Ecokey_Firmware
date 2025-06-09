#ifndef WIFI_STA_H
#define WIFI_STA_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuración para la reconexión automática
 */
typedef struct {
    bool enabled;               /*!< Habilitar/deshabilitar la reconexión automática */
    uint8_t max_retries;        /*!< Número máximo de intentos (0 = infinito) */
    uint16_t initial_interval_ms; /*!< Intervalo inicial entre intentos en ms */
    uint16_t max_interval_ms;   /*!< Intervalo máximo entre intentos en ms */
} wifi_sta_reconnect_config_t;

/**
 * @brief Inicializa el componente WiFi en modo estación
 * 
 * @return esp_err_t ESP_OK si tiene éxito
 */
esp_err_t sta_wifi_init(void);

/**
 * @brief Deinicializa el componente WiFi
 */
void sta_wifi_deinit(void);

/**
 * @brief Devuelve un puntero a la MAC del dispositivo en formato string.
 * @return const char* MAC como string (ej: "AA:BB:CC:DD:EE:FF")
 */
const char *sta_wifi_get_mac_str(void);
const char *sta_wifi_get_mac_clean(void);

/**
 * @brief Conecta a un AP WiFi
 * 
 * @param ssid SSID del AP
 * @param password Contraseña del AP (NULL para redes abiertas)
 * @param timeout_ms Tiempo de espera en ms (0 = esperar indefinidamente)
 * @return esp_err_t 
 */
esp_err_t sta_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms);

/**
 * @brief Conecta a un AP WiFi usando las credenciales almacenadas en NVS
 *        o en Kconfig si no hay credenciales en NVS
 * 
 * @param timeout_ms Tiempo de espera en ms (0 = esperar indefinidamente)
 * @return esp_err_t ESP_OK en caso de éxito, 
 *                  ESP_ERR_NOT_FOUND si no hay credenciales guardadas ni en NVS ni en Kconfig
 */
esp_err_t sta_wifi_connect_with_nvs(uint32_t timeout_ms);

/**
 * @brief Guarda las credenciales WiFi de Kconfig en NVS si no existen ya
 * 
 * @return esp_err_t ESP_OK si se guardaron o ya existían credenciales
 *                   ESP_ERR_NOT_FOUND si no hay credenciales en Kconfig
 */
esp_err_t sta_wifi_save_kconfig_to_nvs(void);

/**
 * @brief Desconecta del AP actual
 * 
 * @return esp_err_t 
 */
esp_err_t sta_wifi_disconnect(void);

/**
 * @brief Configura el comportamiento de reconexión
 * 
 * @param config Configuración de reconexión (NULL para usar valores predeterminados)
 * @return esp_err_t 
 */
esp_err_t sta_wifi_configure_reconnect(const wifi_sta_reconnect_config_t *config);

/**
 * @brief Habilita o deshabilita la reconexión automática
 * 
 * @param enable true para habilitar, false para deshabilitar
 */
void sta_wifi_enable_reconnect(bool enable);

/**
 * @brief Verifica si está conectado a un AP
 * 
 * @return true si está conectado
 */
bool sta_wifi_is_connected(void);

/**
 * @brief Obtiene la dirección IP actual
 * 
 * @param ip_str Buffer para almacenar la IP como string
 * @return esp_err_t 
 */
esp_err_t sta_wifi_get_ip(char *ip_str);

/**
 * @brief Obtiene el RSSI de la conexión actual
 * 
 * @param rssi Puntero para almacenar el valor RSSI
 * @return esp_err_t 
 */
esp_err_t sta_wifi_get_rssi(int8_t *rssi);

/**
 * @brief Verifica si está en proceso de reconexión
 * 
 * @return true si está reconectando
 */
bool sta_wifi_is_reconnecting(void);

/**
 * @brief Configura si ignorar errores temporales de conexión
 * 
 * @param ignore_connection_errors true para ignorar errores temporales
 */
void sta_wifi_set_ignore_connection_errors(bool ignore_connection_errors);

/**
 * @brief Obtiene la dirección MAC de la interfaz WiFi
 * 
 * @param mac_str Buffer para almacenar la dirección MAC como string
 * @param len Longitud del buffer
 * @return esp_err_t 
 */
esp_err_t sta_wifi_get_mac(char *mac_str, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_STA_H */

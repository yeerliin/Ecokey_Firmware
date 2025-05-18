#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el BLE scanner.
 *
 * Inicializa el stack NimBLE, la memoria NVS y lanza el escaneo BLE pasivo.
 * Esta función debe ser llamada una sola vez al inicio del programa.
 */
void ble_scanner_init(void);

/**
 * @brief Consulta si el tag objetivo fue detectado.
 *
 * @return true si el dispositivo con la MAC objetivo ha sido detectado
 *         desde la última consulta. El flag se reinicia después de la llamada.
 */
bool ble_scanner_tag_detected(void);

/**
 * @brief Define la dirección MAC objetivo en formato de array binario.
 *
 * @param mac Puntero a array de 6 bytes que contiene la dirección MAC a buscar.
 */
void ble_scanner_set_target_mac(const uint8_t *mac);

/**
 * @brief Define la dirección MAC objetivo en formato de texto.
 *
 * Acepta formatos como "001B660F4AE2" o "00:1B:66:0F:4A:E2".
 *
 * @param mac_str Cadena de caracteres que representa la MAC.
 * @return true si la conversión y configuración fueron exitosas, false si hubo error de formato.
 */
bool ble_scanner_set_target_mac_str(const char *mac_str);


#ifdef __cplusplus
}
#endif

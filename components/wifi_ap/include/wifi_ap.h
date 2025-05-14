#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicia el ESP32 en modo Access Point
 *
 * @param ssid      Nombre de la red WiFi
 * @param password  Contraseña WPA2 (mínimo 8 caracteres o NULL para red abierta)
 */
void WiFiManager_initAP(const char *ssid, const char *password);

/**
 * @brief Conecta el ESP32 a una red WiFi como estación (modo STA)
 *
 * @param ssid      Nombre de la red WiFi
 * @param password  Contraseña
 */
void WiFiManager_connect(const char *ssid, const char *password);

/**
 * @brief Retorna la dirección MAC del dispositivo como string
 */
const char *WiFiManager_getDeviceID(void);

/**
 * @brief Inicializa mDNS con el nombre de host dado
 *
 * @param hostname  Nombre para mDNS (ej: "esp32")
 */
void WiFiManager_initMDNS(const char *hostname);

#ifdef __cplusplus
}
#endif
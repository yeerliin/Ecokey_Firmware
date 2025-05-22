void func(void);
#pragma once

#include "esp_err.h"

/**
 * @brief Inicializa el cliente MQTT
 * 
 * @return ESP_OK si se inició correctamente
 */
esp_err_t mqtt_client_init(void);

/**
 * @brief Detiene el cliente MQTT
 * 
 * @return ESP_OK si se detuvo correctamente
 */
esp_err_t mqtt_client_stop(void);

/**
 * @brief Publica el estado actual del relé
 * 
 * @return ESP_OK si se publicó correctamente
 */
esp_err_t mqtt_client_publish_relay_state(void);

/**
 * @brief Publica el estado actual de la aplicación
 * 
 * @return ESP_OK si se publicó correctamente
 */
esp_err_t mqtt_client_publish_app_state(void);
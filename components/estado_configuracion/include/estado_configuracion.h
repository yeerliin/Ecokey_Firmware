#pragma once

#include "esp_err.h"

/**
 * @brief Inicia el modo configuración de la aplicación
 * 
 * @return ESP_OK si se inició correctamente
 */
esp_err_t estado_configuracion_iniciar(void);

/**
 * @brief Detiene el modo configuración de la aplicación
 * 
 * @return ESP_OK si se detuvo correctamente
 */
esp_err_t estado_configuracion_detener(void);

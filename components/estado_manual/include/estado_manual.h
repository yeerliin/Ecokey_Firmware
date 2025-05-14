#pragma once

#include "esp_err.h"

/**
 * @brief Inicia el modo normal de la aplicación
 * 
 * @return ESP_OK si se inició correctamente
 */
esp_err_t estado_manual_iniciar(void);

/**
 * @brief Detiene el modo normal de la aplicación
 * 
 * @return ESP_OK si se detuvo correctamente
 */
esp_err_t estado_manual_detener(void);

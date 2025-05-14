#pragma once

#include "esp_err.h"

/**
 * @brief Inicia el modo automático de la aplicación
 * 
 * @return ESP_OK si se inició correctamente
 */
esp_err_t estado_automatico_iniciar(void);

/**
 * @brief Detiene el modo automático de la aplicación
 * 
 * @return ESP_OK si se detuvo correctamente
 */
esp_err_t estado_automatico_detener(void);

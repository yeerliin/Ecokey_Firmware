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

// Permite actualizar el timeout dinámicamente (por ejemplo, desde MQTT)
void estado_automatico_set_timeout_minutos(uint32_t minutos);

/**
 * @brief Obtiene el valor actual del temporizador (en minutos) usado por el modo automático.
 */
uint32_t estado_automatico_get_timeout_minutos(void);

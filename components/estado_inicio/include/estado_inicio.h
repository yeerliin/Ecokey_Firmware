#pragma once

#include "esp_err.h"

/**
 * @brief Inicia las operaciones específicas del primer arranque del dispositivo
 * 
 * Este estado solo se ejecuta una vez cuando el dispositivo se inicia por primera vez
 * o después de un restablecimiento de fábrica. Realiza la configuración inicial
 * y luego cambia automáticamente al siguiente estado apropiado.
 * 
 * @return ESP_OK si se inició correctamente
 */
esp_err_t estado_inicio_iniciar(void);

/**
 * @brief Detiene el estado de primer inicio
 * 
 * @return ESP_OK si se detuvo correctamente
 */
esp_err_t estado_inicio_detener(void);

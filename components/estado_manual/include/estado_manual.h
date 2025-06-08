#pragma once

#include "esp_err.h"
#include <stdbool.h>  // Añadido para usar el tipo bool

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

/**
 * @brief Alterna el estado del relé en modo manual
 * 
 * @return ESP_OK si se realizó la acción correctamente
 */
esp_err_t estado_manual_alternar_rele(void);

/**
 * @brief Obtiene el estado actual del relé
 * 
 * @param estado Puntero donde se almacenará el estado (true=activado, false=desactivado)
 * @return ESP_OK si se obtuvo correctamente
 */
esp_err_t estado_manual_obtener_estado_rele(bool *estado);

/**
 * @brief Verifica si el estado manual está activo
 * 
 * @return true si está activo, false en caso contrario
 */
bool estado_manual_esta_activo(void);

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el controlador del relé con configuración fija.
 * 
 * Configura el pin GPIO como salida para controlar el relé.
 * El pin GPIO y el modo de activación están predefinidos como constantes.
 * 
 * @return
 *      - ESP_OK si la inicialización fue exitosa
 *      - Otros códigos de error en caso de fallo
 */
esp_err_t relay_controller_init(void);

/**
 * @brief Activa el relé.
 * 
 * @return
 *      - ESP_OK si la operación fue exitosa
 *      - ESP_ERR_INVALID_STATE si el controlador no ha sido inicializado
 */
esp_err_t relay_controller_activate(void);

/**
 * @brief Desactiva el relé.
 * 
 * @return
 *      - ESP_OK si la operación fue exitosa
 *      - ESP_ERR_INVALID_STATE si el controlador no ha sido inicializado
 */
esp_err_t relay_controller_deactivate(void);

/**
 * @brief Establece el estado del relé.
 * 
 * @param state true para activar el relé, false para desactivarlo.
 * @return
 *      - ESP_OK si la operación fue exitosa
 *      - ESP_ERR_INVALID_STATE si el controlador no ha sido inicializado
 */
esp_err_t relay_controller_set_state(bool state);

/**
 * @brief Obtiene el estado actual del relé.
 * 
 * @param state Puntero donde se almacenará el estado actual (true = activado, false = desactivado)
 * @return
 *      - ESP_OK si la operación fue exitosa
 *      - ESP_ERR_INVALID_STATE si el controlador no ha sido inicializado
 *      - ESP_ERR_INVALID_ARG si el puntero state es NULL
 */
esp_err_t relay_controller_get_state(bool *state);

/**
 * @brief Genera un pulso en el relé (activa y luego desactiva)
 * 
 * @param duration_ms Duración del pulso en milisegundos
 * @return
 *      - ESP_OK si la operación fue exitosa
 *      - ESP_ERR_INVALID_STATE si el controlador no ha sido inicializado
 */
esp_err_t relay_controller_pulse(uint32_t duration_ms);

#ifdef __cplusplus
}
#endif
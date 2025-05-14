#pragma once


#ifndef LED_H
#define LED_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Tipo de patrón de parpadeo del LED
 */
typedef enum {
    LED_BLINK_NORMAL,           /*!< Parpadeo normal con igual tiempo encendido y apagado */
    LED_BLINK_SOS,              /*!< Patrón de SOS (3 cortos, 3 largos, 3 cortos) */
    LED_BLINK_CUSTOM            /*!< Patrón personalizado definido por el usuario */
} led_blink_pattern_t;

/**
 * @brief Parámetros avanzados para el parpadeo del LED
 */
typedef struct {
    uint32_t on_time_ms;        /*!< Tiempo encendido en milisegundos */
    uint32_t off_time_ms;       /*!< Tiempo apagado en milisegundos */
    uint32_t repeat_count;      /*!< Número de repeticiones (0 = infinito) */
    led_blink_pattern_t pattern; /*!< Tipo de patrón de parpadeo */
} led_blink_params_t;

/**
 * @brief Inicializa el LED con configuración fija
 * 
 * @return esp_err_t ESP_OK si la inicialización es exitosa,
 *                  ESP_FAIL si hay error en configuración de GPIO
 */
esp_err_t led_init(void);

/**
 * @brief Controla el estado del LED
 * 
 * @param on true para encender, false para apagar
 * @return esp_err_t ESP_OK si la operación es exitosa
 *                  ESP_ERR_INVALID_STATE si el LED no está inicializado
 */
esp_err_t led_set(bool on);

/**
 * @brief Alterna el estado del LED (encender si está apagado, apagar si está encendido)
 * 
 * @return esp_err_t ESP_OK si la operación es exitosa
 *                  ESP_ERR_INVALID_STATE si el LED no está inicializado
 */
esp_err_t led_toggle(void);

/**
 * @brief Obtiene el estado actual del LED
 * 
 * @return bool true si está encendido, false si está apagado
 */
bool led_get_state(void);

/**
 * @brief Inicia el parpadeo del LED con un intervalo específico
 * 
 * @param interval_ms Intervalo de parpadeo en milisegundos (tiempo encendido = tiempo apagado)
 * @return esp_err_t ESP_OK si la operación es exitosa
 *                  ESP_ERR_INVALID_STATE si el LED no está inicializado
 */
esp_err_t led_blink_start(uint32_t interval_ms);

/**
 * @brief Inicia el parpadeo del LED con parámetros avanzados
 * 
 * @param params Parámetros avanzados del parpadeo
 * @return esp_err_t ESP_OK si la operación es exitosa
 *                  ESP_ERR_INVALID_STATE si el LED no está inicializado
 *                  ESP_ERR_INVALID_ARG si algún parámetro es inválido
 */
esp_err_t led_blink_advanced(const led_blink_params_t* params);

/**
 * @brief Detiene el parpadeo del LED y lo deja apagado
 * 
 * @return esp_err_t ESP_OK si la operación es exitosa
 *                  ESP_ERR_INVALID_STATE si el LED no está inicializado
 */
esp_err_t led_blink_stop(void);

/**
 * @brief Libera los recursos utilizados por el componente LED
 * 
 * @return esp_err_t ESP_OK si la operación es exitosa
 */
esp_err_t led_deinit(void);

#endif // LED_H

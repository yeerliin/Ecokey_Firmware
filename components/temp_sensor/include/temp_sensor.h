#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el componente de temperatura
 * 
 * @return esp_err_t ESP_OK en caso de éxito
 */
esp_err_t temp_sensor_init(void);

/**
 * @brief Inicia las lecturas periódicas de temperatura
 * 
 * @return esp_err_t ESP_OK en caso de éxito
 */
esp_err_t temp_sensor_start(void);

/**
 * @brief Detiene las lecturas periódicas de temperatura
 * 
 * @return esp_err_t ESP_OK en caso de éxito
 */
esp_err_t temp_sensor_stop(void);

/**
 * @brief Obtiene la última temperatura leída
 * 
 * @param temperature Puntero donde se guardará el valor
 * @return esp_err_t ESP_OK en caso de éxito
 */
esp_err_t temp_sensor_get_last_temp(float *temperature);

/**
 * @brief Configura el intervalo de lectura de temperatura
 * 
 * @param interval_ms Intervalo en milisegundos (mínimo 1000ms)
 * @return esp_err_t ESP_OK en caso de éxito
 */
esp_err_t temp_sensor_set_interval(uint32_t interval_ms);

/**
 * @brief Libera todos los recursos del componente
 * 
 * @return esp_err_t ESP_OK en caso de éxito
 */
esp_err_t temp_sensor_deinit(void);

#ifdef __cplusplus
}
#endif

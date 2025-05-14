#pragma once


#ifndef APP_INICIALIZACION_H
#define APP_INICIALIZACION_H

#include "esp_err.h"

/**
 * @brief Inicializa todos los componentes del sistema
 * 
 * @return esp_err_t ESP_OK si todos los componentes críticos se inicializaron correctamente,
 *                   código de error específico en caso contrario
 */
esp_err_t inicializar_componentes(void);

#endif // APP_INICIALIZACION_H
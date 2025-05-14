// control_button.h

#pragma once

#include "esp_err.h"

/**
 * @brief Inicializa la lógica de control del botón.
 *
 * Esta función registra el callback que escucha eventos del botón y aplica la lógica
 * de alternar entre CONFIGURACION y el estado anterior.
 */
esp_err_t control_button_iniciar(void);

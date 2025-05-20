#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tipos de eventos del botón
 * 
 * Esta enumeración define los diferentes tipos de eventos que puede generar el botón.
 */
typedef enum {
    BOTON_PULSACION_SIMPLE,      /*!< Evento cuando se detecta una pulsación corta */
    BOTON_DOBLE_PULSACION,       /*!< Evento cuando se detecta una doble pulsación rápida */
    BOTON_PULSACION_LARGA,       /*!< Evento cuando se detecta una pulsación de 3 segundos */
    BOTON_PULSACION_MUY_LARGA,   /*!< Evento cuando se detecta una pulsación de 7 segundos */
    BOTON_PULSACION_RESET,       /*!< Evento cuando se detecta una pulsación de 12 segundos */
    BOTON_PRESIONADO,            /*!< Evento cuando el botón se acaba de presionar */
    BOTON_LIBERADO,              /*!< Evento cuando el botón se acaba de liberar */
} tipo_evento_boton_t;

/**
 * @note Una doble pulsación rápida se detecta si se presiona dos veces el botón con menos de 400 ms entre liberaciones.
 */

/**
 * @brief Función de callback para eventos del botón
 * 
 * @param evento Tipo de evento ocurrido
 */
typedef void (*funcion_callback_boton_t)(tipo_evento_boton_t evento);

/**
 * @brief Inicializa el botón con el callback proporcionado
 * 
 * Esta función inicia el botón con la configuración predeterminada.
 * El botón se configura en el GPIO definido internamente en el componente.
 * 
 * @param[in] callback Función que será llamada cuando ocurra un evento
 * @return
 *      - ESP_OK: Inicialización exitosa
 *      - ESP_ERR_INVALID_STATE: Componente ya inicializado
 *      - ESP_ERR_NO_MEM: Error de asignación de memoria
 */
esp_err_t iniciar_boton(funcion_callback_boton_t callback);

/**
 * @brief Detiene y libera los recursos del botón
 * 
 * Esta función detiene el botón y libera todos los recursos utilizados.
 */
void detener_boton(void);

/**
 * @brief Verifica si el botón está presionado
 * 
 * @return
 *      - true: El botón está presionado
 *      - false: El botón no está presionado o no está inicializado
 */
bool boton_esta_presionado(void);

#ifdef __cplusplus
}
#endif

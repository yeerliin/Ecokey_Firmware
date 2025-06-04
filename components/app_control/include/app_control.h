#pragma once

#include "esp_err.h"

// Definición de estados posibles
typedef enum {
    ESTADO_CONFIGURACION = 1,
    ESTADO_MANUAL = 2,
    ESTADO_AUTOMATICO = 3, 
    ESTADO_INVALIDO = 255,
    
    
    // Puedes añadir más estados según necesites
} estado_app_t;

/**
 * @brief Inicializa el sistema de control de estados
 * 
 * Carga el estado guardado en NVS o determina si es primera ejecución
 * 
 * @return ESP_OK en caso de éxito
 */
esp_err_t app_control_iniciar(void);

/**
 * @brief Cambia el estado actual del dispositivo
 * 
 * @param nuevo_estado El estado al que se debe cambiar
 * @return ESP_OK en caso de éxito
 */
esp_err_t app_control_cambiar_estado(estado_app_t nuevo_estado);

/**
 * @brief Obtiene el estado actual del dispositivo
 * 
 * @return Estado actual
 */
estado_app_t app_control_obtener_estado_actual(void);

/**
 * @brief Guarda el estado actual en NVS
 * 
 * @return ESP_OK en caso de éxito
 */
esp_err_t app_control_guardar_estado(void);

/**
 * @brief Inicia el estado adecuado basado en la configuración guardada
 * 
 * Esta función decide qué estado iniciar basándose en:
 * - Si hay un estado previamente guardado en NVS
 * - Si no hay estado guardado, usa lógica predeterminada
 * 
 * @return ESP_OK si se inició correctamente un estado
 */
esp_err_t app_control_iniciar_estado(void);

/**
 * @brief Lanza una transición de estado en una tarea genérica
 *
 * Esta función maneja las transiciones de estado de forma asíncrona y segura:
 * - Filtra solicitudes redundantes al mismo destino
 * - Previene sobrecarga de la cola de transiciones
 * - Maneja automáticamente múltiples solicitudes para el mismo estado
 *
 * @param destino Estado al que se quiere transicionar
 * @param tag Nombre para logging (usualmente el TAG del archivo actual)
 * @return ESP_OK si la tarea fue lanzada correctamente o si ya existe una transición igual
 */
esp_err_t app_control_lanzar_transicion(estado_app_t destino, const char *tag);

/**
 * Obtiene el estado actual de la aplicación.
 *
 * @return Estado actual del sistema.
 */
estado_app_t app_control_get_estado(void);

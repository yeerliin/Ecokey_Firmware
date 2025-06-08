#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tipos de estado para configuración específica de recursos
 */
typedef enum {
    RESOURCE_TYPE_MANUAL,       // Estado manual - recursos mínimos
    RESOURCE_TYPE_AUTOMATICO,   // Estado automático - BLE + tareas
    RESOURCE_TYPE_CONFIGURACION // Estado configuración - Web server
} resource_type_t;

/**
 * @brief Configuración de recursos para cada tipo
 */
typedef struct {
    uint32_t min_heap_required;    // Memoria heap mínima requerida
    uint32_t min_stack_size;       // Tamaño mínimo de stack para tareas
    uint32_t warning_heap_level;   // Nivel de advertencia de memoria baja
    const char* component_name;    // Nombre del componente para logs
} resource_config_t;

/**
 * @brief Contexto de recursos para un componente
 */
typedef struct {
    resource_type_t type;
    resource_config_t config;
    TaskHandle_t* task_handle;     // Handle de tarea principal (opcional)
    bool is_active;
    size_t initial_heap;           // Heap al iniciar
} resource_context_t;

/**
 * @brief Inicializa el gestor de recursos
 */
esp_err_t resource_manager_init(void);

/**
 * @brief Crea un contexto de recursos para un componente
 * 
 * @param type Tipo de recurso
 * @param task_handle Puntero al handle de tarea (puede ser NULL)
 * @param context Contexto de salida
 * @return ESP_OK si se creó correctamente
 */
esp_err_t resource_manager_create_context(resource_type_t type, 
                                        TaskHandle_t* task_handle,
                                        resource_context_t* context);

/**
 * @brief Valida que haya recursos suficientes antes de iniciar
 * 
 * @param context Contexto de recursos
 * @return ESP_OK si hay recursos suficientes
 */
esp_err_t resource_manager_validate(resource_context_t* context);

/**
 * @brief Monitorea el uso de memoria en un punto específico
 * 
 * @param context Contexto de recursos
 * @param checkpoint Nombre del punto de control
 */
void resource_manager_monitor(resource_context_t* context, const char* checkpoint);

/**
 * @brief Limpia recursos de un contexto
 * 
 * @param context Contexto de recursos
 * @param cleanup_callback Función de cleanup específica del componente (opcional)
 */
void resource_manager_cleanup(resource_context_t* context, void (*cleanup_callback)(void));

/**
 * @brief Marca el contexto como activo
 * 
 * @param context Contexto de recursos
 */
void resource_manager_set_active(resource_context_t* context, bool active);

/**
 * @brief Verifica si hay una fuga de memoria comparando con el estado inicial
 * 
 * @param context Contexto de recursos
 * @return true si se detectó fuga de memoria
 */
bool resource_manager_check_memory_leak(resource_context_t* context);

/**
 * @brief Obtiene estadísticas de memoria del sistema
 * 
 * @param free_heap Heap libre actual
 * @param min_heap Heap mínimo histórico
 * @param largest_block Bloque más grande disponible
 */
void resource_manager_get_memory_stats(size_t* free_heap, size_t* min_heap, size_t* largest_block);

#ifdef __cplusplus
}
#endif

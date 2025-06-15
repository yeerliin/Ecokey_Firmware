#include "resource_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "RESOURCE_MGR";

// Configuraciones predefinidas para cada tipo
static const resource_config_t resource_configs[] = {
    [RESOURCE_TYPE_MANUAL] = {
        .min_heap_required = 10 * 1024,    // 10KB
        .min_stack_size = 2048,
        .warning_heap_level = 5 * 1024,    // 5KB
        .component_name = "MANUAL"
    },
    [RESOURCE_TYPE_AUTOMATICO] = {
        .min_heap_required = 20 * 1024,    // 20KB
        // Automatico task observed <1k words usage, keep some margin
        .min_stack_size = 2048,
        .warning_heap_level = 10 * 1024,   // 10KB
        .component_name = "AUTOMATICO"
    },
    [RESOURCE_TYPE_CONFIGURACION] = {
        .min_heap_required = 30 * 1024,    // 30KB
        .min_stack_size = 2048,
        .warning_heap_level = 15 * 1024,   // 15KB
        .component_name = "CONFIGURACION"
    }
};

esp_err_t resource_manager_init(void) {
    ESP_LOGI(TAG, "Inicializando gestor de recursos");
    
    size_t free_heap, min_heap, largest_block;
    resource_manager_get_memory_stats(&free_heap, &min_heap, &largest_block);
    
    ESP_LOGI(TAG, "Estado inicial - Heap libre: %zu, mínimo: %zu, bloque mayor: %zu", 
             free_heap, min_heap, largest_block);
    
    return ESP_OK;
}

esp_err_t resource_manager_create_context(resource_type_t type, 
                                        TaskHandle_t* task_handle,
                                        resource_context_t* context) {
    if (context == NULL || type >= sizeof(resource_configs)/sizeof(resource_configs[0])) {
        return ESP_ERR_INVALID_ARG;
    }
    
    context->type = type;
    context->config = resource_configs[type];
    context->task_handle = task_handle;
    context->is_active = false;
    context->initial_heap = esp_get_free_heap_size();
    
    ESP_LOGI(TAG, "Contexto creado para %s - Heap inicial: %zu bytes", 
             context->config.component_name, context->initial_heap);
    
    return ESP_OK;
}

esp_err_t resource_manager_validate(resource_context_t* context) {
    if (context == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    const char* component = context->config.component_name;
    
    ESP_LOGI(TAG, "=== VALIDANDO RECURSOS PARA %s ===", component);
    
    // Verificar memoria heap disponible
    size_t free_heap = esp_get_free_heap_size();
    size_t min_heap = esp_get_minimum_free_heap_size();
    
    ESP_LOGI(TAG, "[%s] Heap libre: %zu bytes, mínimo histórico: %zu bytes", 
             component, free_heap, min_heap);
    
    if (free_heap < context->config.min_heap_required) {
        ESP_LOGE(TAG, "[%s] Memoria insuficiente. Requerido: %lu bytes, disponible: %zu bytes", 
                 component, context->config.min_heap_required, free_heap);
        return ESP_ERR_NO_MEM;
    }
    
    // Verificar que no hay tareas duplicadas (si aplica)
    if (context->task_handle != NULL && *context->task_handle != NULL) {
        ESP_LOGW(TAG, "[%s] Task handle ya existe, posible leak de recursos", component);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Verificar stack de tarea actual
    UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "[%s] Stack libre de tarea actual: %u words", component, stack_high_water);
    
    // Verificar fragmentación de memoria
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    if (largest_block < context->config.min_heap_required) {
        ESP_LOGW(TAG, "[%s] Fragmentación detectada. Bloque mayor: %zu bytes", 
                 component, largest_block);
    }
    
    ESP_LOGI(TAG, "[%s] Validación de recursos EXITOSA", component);
    return ESP_OK;
}

void resource_manager_monitor(resource_context_t* context, const char* checkpoint) {
    if (context == NULL || checkpoint == NULL) {
        return;
    }
    
    size_t free_heap = esp_get_free_heap_size();
    size_t min_heap = esp_get_minimum_free_heap_size();
    const char* component = context->config.component_name;
    
    ESP_LOGD(TAG, "[%s:%s] Heap libre: %zu, mínimo: %zu", component, checkpoint, free_heap, min_heap);
    
    if (free_heap < context->config.warning_heap_level) {
        ESP_LOGW(TAG, "[%s:%s] MEMORIA BAJA detectada: %zu bytes", component, checkpoint, free_heap);
    }
    
    // Verificar stack si hay tarea activa
    if (context->task_handle != NULL && *context->task_handle != NULL) {
        UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(*context->task_handle);
        if (stack_remaining < 512) { // Menos de 512 words libres
            ESP_LOGW(TAG, "[%s:%s] Stack bajo en tarea: %u words restantes", 
                     component, checkpoint, stack_remaining);
        }
    }
}

void resource_manager_cleanup(resource_context_t* context, void (*cleanup_callback)(void)) {
    if (context == NULL) {
        return;
    }
    
    const char* component = context->config.component_name;
    ESP_LOGI(TAG, "[%s] Iniciando cleanup de recursos", component);
    
    // Log de memoria antes del cleanup
    size_t heap_before = esp_get_free_heap_size();
    ESP_LOGI(TAG, "[%s] Memoria antes del cleanup: %zu bytes", component, heap_before);
    
    // Ejecutar cleanup específico del componente
    if (cleanup_callback != NULL) {
        ESP_LOGD(TAG, "[%s] Ejecutando cleanup específico", component);
        cleanup_callback();
    }
    
    // Esperar a que la tarea termine si existe
    if (context->task_handle != NULL && *context->task_handle != NULL) {
        ESP_LOGI(TAG, "[%s] Esperando finalización de tarea", component);
        int timeout_count = 0;
        while (*context->task_handle != NULL && timeout_count < 100) { // 5 segundos max
            vTaskDelay(pdMS_TO_TICKS(50));
            timeout_count++;
        }
        
        if (*context->task_handle != NULL) {
            ESP_LOGW(TAG, "[%s] Timeout esperando finalización de tarea", component);
        } else {
            ESP_LOGI(TAG, "[%s] Tarea finalizada correctamente", component);
        }
    }
    
    // Log de memoria después del cleanup
    size_t heap_after = esp_get_free_heap_size();
    ESP_LOGI(TAG, "[%s] Memoria después del cleanup: %zu bytes", component, heap_after);
    
    // Verificar si se liberó memoria
    if (heap_after > heap_before) {
        ESP_LOGI(TAG, "[%s] Memoria liberada: %zu bytes", component, heap_after - heap_before);
    } else if (heap_after < heap_before) {
        ESP_LOGW(TAG, "[%s] Posible fuga de memoria: %zu bytes", component, heap_before - heap_after);
    }
    
    context->is_active = false;
    ESP_LOGI(TAG, "[%s] Cleanup completado", component);
}

void resource_manager_set_active(resource_context_t* context, bool active) {
    if (context != NULL) {
        context->is_active = active;
        ESP_LOGD(TAG, "[%s] Estado cambiado a: %s", 
                 context->config.component_name, active ? "ACTIVO" : "INACTIVO");
    }
}

bool resource_manager_check_memory_leak(resource_context_t* context) {
    if (context == NULL) {
        return false;
    }
    
    size_t current_heap = esp_get_free_heap_size();
    const char* component = context->config.component_name;
    
    if (current_heap < context->initial_heap) {
        size_t leaked = context->initial_heap - current_heap;
        ESP_LOGW(TAG, "[%s] Posible fuga de memoria: %zu bytes no liberados", component, leaked);
        return true;
    }
    
    ESP_LOGD(TAG, "[%s] Sin fugas de memoria detectadas", component);
    return false;
}

void resource_manager_get_memory_stats(size_t* free_heap, size_t* min_heap, size_t* largest_block) {
    if (free_heap) *free_heap = esp_get_free_heap_size();
    if (min_heap) *min_heap = esp_get_minimum_free_heap_size();
    if (largest_block) *largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
}

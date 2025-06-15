#include "estado_manual.h"
#include "esp_log.h"
#include "relay_controller.h"
#include "mqtt_service.h"
#include "wifi_sta.h"
#include "time_manager.h"
#include "led.h"
#include "resource_manager.h" // Nuevo componente de gestión de recursos

static const char *TAG = "ESTADO_MANUAL";
static bool estado_activo = false;
static resource_context_t resource_ctx; // Contexto de recursos

/**
 * @brief Cleanup específico para el estado manual
 */
static void cleanup_manual(void) {
    // Asegurar relé desactivado
    relay_controller_set_state(false);
    ESP_LOGI(TAG, "Relé desactivado");
}

esp_err_t estado_manual_iniciar(void) {
    // Crear contexto de recursos
    esp_err_t ret = resource_manager_create_context(RESOURCE_TYPE_MANUAL, 
                                                   NULL, // No hay tarea en manual
                                                   &resource_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al crear contexto de recursos: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Validar recursos antes de iniciar
    ret = resource_manager_validate(&resource_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Validación de recursos falló: %s", esp_err_to_name(ret));
        return ret;
    }
    
    if (estado_activo) {
        ESP_LOGW(TAG, "Estado manual ya está activo");
        return ESP_OK;
    } else {
        const char *mac_topic = sta_wifi_get_mac_clean(); // MAC sin dos puntos para el topic
        char topic[64];
        snprintf(topic, sizeof(topic), "dispositivos/%s/modo", mac_topic);
        char fecha_actual[24];
        if (time_manager_get_fecha_actual(fecha_actual, sizeof(fecha_actual)) == ESP_OK)
        {
            mqtt_service_enviar_json(topic, 2, 1, "Modo", "manual", "FechaModo", fecha_actual, NULL);
            printf("Fecha actual: %s\n", fecha_actual);
        }
        else
        {
            mqtt_service_enviar_json(topic, 2, 1, "Modo", "manual", NULL);
        }
    }
    
    resource_manager_monitor(&resource_ctx, "inicio");
    led_blink_stop(); 
    ESP_LOGI(TAG, "Iniciando el modo manual");
    
    // === POLÍTICA DEL MODO MANUAL ===
    // En modo manual, el relé empieza APAGADO y el usuario lo controla manualmente
    // Esto asegura que tras un reinicio, el relé no se quede encendido sin control
    relay_controller_set_state(false);
    ESP_LOGI(TAG, "Relé inicializado en APAGADO para control manual");
    
    estado_activo = true;
    resource_manager_set_active(&resource_ctx, true);
    
    resource_manager_monitor(&resource_ctx, "post-init");
    ESP_LOGI(TAG, "=== ESTADO MANUAL INICIADO CORRECTAMENTE ===");
    
    return ESP_OK;
}

esp_err_t estado_manual_detener(void) {
    if (!estado_activo) {
        ESP_LOGW(TAG, "Estado manual no está activo");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "=== DETENIENDO ESTADO MANUAL ===");
    resource_manager_monitor(&resource_ctx, "pre-detener");
    
    // Cleanup usando el gestor de recursos
    resource_manager_cleanup(&resource_ctx, cleanup_manual);
    
    estado_activo = false;
    
    // Verificar fugas de memoria
    resource_manager_check_memory_leak(&resource_ctx);
    
    resource_manager_monitor(&resource_ctx, "post-detener");
    ESP_LOGI(TAG, "=== ESTADO MANUAL DETENIDO ===");
    
    return ESP_OK;
}

esp_err_t estado_manual_alternar_rele(void) {
    if (!estado_activo) {
        ESP_LOGW(TAG, "No se puede alternar relé: estado manual no activo");
        return ESP_ERR_INVALID_STATE;
    } 
    
    bool estado_actual;
    esp_err_t ret = relay_controller_get_state(&estado_actual);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al obtener estado del relé: %s", esp_err_to_name(ret));
        return ret;
    }
    
    bool nuevo_estado = !estado_actual;
    ret = relay_controller_set_state(nuevo_estado);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Relé %s", nuevo_estado ? "ACTIVADO" : "DESACTIVADO");
        
        // Aquí podrías añadir la notificación MQTT u otras acciones
        // cuando se modifica el estado del relé
    }
    
    return ret;
}

esp_err_t estado_manual_obtener_estado_rele(bool *estado) {
    if (estado == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return relay_controller_get_state(estado);
}

bool estado_manual_esta_activo(void) {
    return estado_activo;
}

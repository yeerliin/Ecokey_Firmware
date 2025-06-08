#include "estado_configuracion.h"
#include "esp_log.h"
#include "esp_event.h"
#include <stdbool.h>
#include "app_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "wifi_provision_web.h"
#include "nvs_manager.h"
#include "resource_manager.h" // Nuevo componente de gestión de recursos
#include "relay_controller.h" // Añadido para controlar el relé

static const char *TAG = "ESTADO_CONFIG";
static bool estado_activo = false;
static resource_context_t resource_ctx; // Contexto de recursos

/**
 * @brief Cleanup específico para el estado de configuración
 */
static void cleanup_configuracion(void) {
    // Detener el portal web si está activo
    if (wifi_provision_web_is_running()) {
        ESP_LOGI(TAG, "Deteniendo el portal de provisioning");
        wifi_provision_web_stop();
    }
    
    // Detener parpadeo del LED
    led_blink_stop();
    ESP_LOGI(TAG, "LED parpadeo detenido");
}

void info_NVS() {
    ESP_LOGI(TAG, "========== CONFIGURACIÓN COMPLETADA ==========");
    
    // Buffer para almacenar los valores leídos de NVS
    char valor[64] = {0};
    
    // Leer y mostrar mac_objetivo
    if (nvs_manager_get_string("mac_objetivo", valor, sizeof(valor)) == ESP_OK) {
        ESP_LOGI(TAG, "MAC Objetivo: %s", valor);
    } else {
        ESP_LOGI(TAG, "MAC Objetivo: [no encontrado]");
    }
    
    // Leer y mostrar SSID
    if (nvs_manager_get_string("ssid", valor, sizeof(valor)) == ESP_OK) {
        ESP_LOGI(TAG, "SSID WiFi: %s", valor);
    } else {
        ESP_LOGI(TAG, "SSID WiFi: [no encontrado]");
    }
    
    // Leer y mostrar password (por seguridad solo mostramos que existe)
    if (nvs_manager_get_string("password", valor, sizeof(valor)) == ESP_OK) {
        ESP_LOGI(TAG, "Password WiFi: [guardado]");
    } else {
        ESP_LOGI(TAG, "Password WiFi: [no encontrado]");
    }
    
    // Leer y mostrar temporizador
    if (nvs_manager_get_string("temporizador", valor, sizeof(valor)) == ESP_OK) {
        ESP_LOGI(TAG, "Temporizador: %s", valor);
    } else {
        ESP_LOGI(TAG, "Temporizador: [no encontrado]");
    }

    ESP_LOGI(TAG, "==============================================");
}

static void provision_completed_callback(void) {
    ESP_LOGI(TAG, "Provisión WiFi completada. Iniciando transición a ESTADO_AUTOMATICO");
    
    // Mostrar información de NVS (si lo deseas)
    info_NVS();
    // Guardar directamente el estado en NVS para que se cargue tras reinicio
    esp_err_t err = nvs_manager_set_u8("app_estado", ESTADO_AUTOMATICO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al guardar estado en NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Estado AUTOMATICO guardado en NVS correctamente");
    }
}

esp_err_t estado_configuracion_iniciar(void) {
    // Crear contexto de recursos
    esp_err_t ret = resource_manager_create_context(RESOURCE_TYPE_CONFIGURACION, 
                                                   NULL, // No hay tarea específica
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
        ESP_LOGW(TAG, "Estado configuración ya está activo");
        return ESP_OK;
    }
    
    resource_manager_monitor(&resource_ctx, "inicio");
    ESP_LOGI(TAG, "Iniciando el modo configuración");
    estado_activo = true;
    resource_manager_set_active(&resource_ctx, true);

    // === POLÍTICA DEL MODO CONFIGURACIÓN ===
    // En modo configuración, el relé debe estar APAGADO
    // No tiene sentido que esté encendido durante la configuración
    relay_controller_set_state(false);
    ESP_LOGI(TAG, "Relé desactivado durante configuración");

    // Iniciar parpadeo del LED (ejemplo: 500 ms)
    led_blink_start(500);
    
    resource_manager_monitor(&resource_ctx, "post-led");
    
    // Configurar el portal web de provisioning
    wifi_prov_web_config_t cfg = {
        .ap_pass = "ecokey123",
        .ap_channel = 1,
        .ap_max_conn = 4,
        .on_provisioned = provision_completed_callback
    };
    
    // Iniciar el portal web de provisioning
    ret = wifi_provision_web_start(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al iniciar el portal de provisioning: %s", esp_err_to_name(ret));
        resource_manager_cleanup(&resource_ctx, cleanup_configuracion);
        estado_activo = false;
        return ret;
    }
    
    resource_manager_monitor(&resource_ctx, "post-web-server");
    ESP_LOGI(TAG, "Portal de configuración WiFi iniciado");
    ESP_LOGI(TAG, "=== ESTADO CONFIGURACIÓN INICIADO CORRECTAMENTE ===");
    
    return ESP_OK;
}

esp_err_t estado_configuracion_detener(void) {
    if (!estado_activo) {
        ESP_LOGW(TAG, "Estado configuración no está activo");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "=== DETENIENDO ESTADO CONFIGURACIÓN ===");
    resource_manager_monitor(&resource_ctx, "pre-detener");

    // Cleanup usando el gestor de recursos
    resource_manager_cleanup(&resource_ctx, cleanup_configuracion);
    
    // Asegúrate de eliminar el event loop antes de salir
    esp_event_loop_delete_default();

    estado_activo = false;
    
    // Verificar fugas de memoria
    resource_manager_check_memory_leak(&resource_ctx);
    
    resource_manager_monitor(&resource_ctx, "post-detener");
    ESP_LOGI(TAG, "=== ESTADO CONFIGURACIÓN DETENIDO ===");
    
    return ESP_OK;
}

#include "estado_configuracion.h"
#include "esp_log.h"
#include <stdbool.h>  // Añadido para tipos bool, true, false
#include "app_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h" // Añadido para controlar el LED
#include "wifi_provision_web.h"
#include "nvs_manager.h" // Añadido para la gestión de NVS


static const char *TAG = "ESTADO_CONFIG";
static bool estado_activo = false;

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
    
    if (estado_activo) {
        ESP_LOGW(TAG, "Estado configuración ya está activo");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Iniciando el modo configuración");
    estado_activo = true;

    // Iniciar parpadeo del LED (ejemplo: 500 ms)
    led_blink_start(500);
    
    // Configurar el portal web de provisioning
    wifi_prov_web_config_t cfg = {
        .ap_pass = "ecokey123",
        .ap_channel = 1,
        .ap_max_conn = 4,
        .on_provisioned = provision_completed_callback
    };
    
    // Iniciar el portal web de provisioning
    esp_err_t ret = wifi_provision_web_start(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al iniciar el portal de provisioning: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Portal de configuración WiFi iniciado");
    return ESP_OK;
}

    esp_err_t estado_configuracion_detener(void) {
    if (!estado_activo) {
        ESP_LOGW(TAG, "Estado configuración no está activo");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deteniendo el modo configuración");

    if (wifi_provision_web_is_running()) {
        ESP_LOGI(TAG, "Deteniendo el portal de provisioning");
        wifi_provision_web_stop(); // Detener el servicio de provisioning
    } else {
        ESP_LOGW(TAG, "El portal de provisioning no está activo");
    }
        // Detener parpadeo del LED
    led_blink_stop();

    estado_activo = false;
    return ESP_OK;
}

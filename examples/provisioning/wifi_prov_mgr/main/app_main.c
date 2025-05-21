#include "wifi_provision_web.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_manager.h"

static const char *TAG = "MAIN";

void mi_callback() {
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

wifi_prov_web_config_t cfg = {
    .ap_pass = "ecokey123",
    .ap_channel = 1,
    .ap_max_conn = 4,
    .on_provisioned = mi_callback
};



void app_main(void) {

    
    wifi_provision_web_start(&cfg);
    ESP_LOGI("MAIN1", "INICIADO..."); 
    vTaskDelay(pdMS_TO_TICKS(1000));  
}


#include "estado_configuracion.h"
#include "esp_log.h"
#include <stdbool.h>  // Añadido para tipos bool, true, false
#include "app_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h" // Añadido para controlar el LED
#include "provisioning_wifi.h" // Añadido para el manejo del WiFi provisioning



static const char *TAG = "ESTADO_CONFIG";
static bool estado_activo = false;



esp_err_t estado_configuracion_iniciar(void) {
    if (estado_activo) {
        ESP_LOGW(TAG, "Estado configuración ya está activo");
        return ESP_OK;
    }
    
    if(!provisioning_wifi_is_done()) {
        ESP_LOGW(TAG, "El provisioning no está completo");
        return ESP_FAIL;
    }else{
        ESP_LOGI(TAG, "El provisioning está completo");
    }
    provisioning_wifi_init();   // por única vez
    provisioning_wifi_start();  // entra al modo BLE o SoftAP
    
   
    ESP_LOGI(TAG, "Iniciando el modo configuración");
    estado_activo = true;

    // Iniciar parpadeo del LED (ejemplo: 500 ms)
    led_blink_start(500);

    return ESP_OK;
}

esp_err_t estado_configuracion_detener(void) {
    if (!estado_activo) {
        ESP_LOGW(TAG, "Estado configuración no está activo");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deteniendo el modo configuración");
    
    // Detener parpadeo del LED
    led_blink_stop();

    estado_activo = false;
    return ESP_OK;
}

#include "estado_configuracion.h"
#include "esp_log.h"
#include <stdbool.h>  // Añadido para tipos bool, true, false
#include "app_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h" // Añadido para controlar el LED
#include "wifi_ap.h" 



static const char *TAG = "ESTADO_CONFIG";
static bool estado_activo = false;



esp_err_t estado_configuracion_iniciar(void) {
    if (estado_activo) {
        ESP_LOGW(TAG, "Estado configuración ya está activo");
        return ESP_OK;
    }
    
    // Iniciar AP sin contraseña para evitar problemas de autenticación
    WiFiManager_initAP("ESP32-Config", "123456789");
    
   
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

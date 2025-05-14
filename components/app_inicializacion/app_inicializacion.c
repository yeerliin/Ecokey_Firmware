#include "app_inicializacion.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_manager.h" 
#include "control_button.h"  
#include "ble_scanner.h"
#include "app_control.h"  
#include "led.h"
#include "relay_controller.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "APP_INIT";

esp_err_t inicializar_componentes(void)
{
    esp_err_t ret;
    
    // 1. Inicializar NVS (paso crítico)
    ret = nvs_manager_init(NULL); // Usar namespace por defecto
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error crítico al inicializar NVS Manager");
        return ret;
    }

    // Pausa más larga para asegurar que NVS esté listo
    vTaskDelay(pdMS_TO_TICKS(500));

    // 2. Inicializar LED
    ESP_LOGI(TAG, "Inicializando LED...");
    ret = led_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar LED: %s", esp_err_to_name(ret));
        return ret;
    }

    // 3. Inicializar Relay Controller
    ESP_LOGI(TAG, "Inicializando Relay Controller...");
    ret = relay_controller_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar Relay Controller: %s", esp_err_to_name(ret));
        return ret;
    }

    // Pausa corta
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 5. Inicializar BLE Scanner
    ESP_LOGI(TAG, "Inicializando escáner BLE...");
    ret = ble_scanner_iniciar(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error crítico al inicializar el escáner BLE: %s", esp_err_to_name(ret));
        return ret;
    }
    // Pausa corta
    vTaskDelay(pdMS_TO_TICKS(100));

         
    // 6. Inicializar el control boton (solo inicialización interna)
    ESP_LOGI(TAG, "Inicializando control por boton...");
    ret = control_button_iniciar(); 
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar el controlador de boton: %s", esp_err_to_name(ret));
        return ret;
    }

    // Pausa corta
    vTaskDelay(pdMS_TO_TICKS(100));

    // 7. Inicializar el controlador de aplicación (solo inicialización interna)
    ESP_LOGI(TAG, "Inicializando controlador de aplicación...");
    ret = app_control_iniciar_estado();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar el controlador de aplicación: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Todos los componentes inicializados correctamente");
    return ESP_OK;
}


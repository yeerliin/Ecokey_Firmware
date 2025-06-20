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
#include "esp_tls.h"

static const char *TAG = "APP_INIT";

extern const uint8_t ca_pem_start[] asm("_binary_ca_pem_start");
extern const uint8_t ca_pem_end[]   asm("_binary_ca_pem_end");

void inicializar_certificados_globales(void);

esp_err_t inicializar_componentes(void)
{
    esp_err_t ret;

    // 1. Inicializar NVS (paso crítico)
    ret = nvs_manager_init(NULL); // Usar namespace por defecto
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error crítico al inicializar NVS Manager");
        return ret;
    }

    // Pausa más larga para asegurar que NVS esté listo
    vTaskDelay(pdMS_TO_TICKS(500));

    // 2. Inicializar LED
    ESP_LOGI(TAG, "Inicializando LED...");
    ret = led_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al inicializar LED: %s", esp_err_to_name(ret));
        return ret;
    }
    led_blink_start(100);

    // 3. Inicializar Relay Controller
    ESP_LOGI(TAG, "Inicializando Relay Controller...");
    ret = relay_controller_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al inicializar Relay Controller: %s", esp_err_to_name(ret));
        return ret;
    }

    // Pausa corta
    vTaskDelay(pdMS_TO_TICKS(100));

    // 6. Inicializar el control boton (solo inicialización interna)
    ESP_LOGI(TAG, "Inicializando control por boton...");
    ret = control_button_iniciar();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al inicializar el controlador de boton: %s", esp_err_to_name(ret));
        return ret;
    }

    // Pausa corta
    vTaskDelay(pdMS_TO_TICKS(100));

    inicializar_certificados_globales();
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Todos los componentes inicializados correctamente");
    return ESP_OK;
}

void inicializar_certificados_globales(void)
{
    esp_err_t err = esp_tls_init_global_ca_store();
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "Error al inicializar CA store: %s", esp_err_to_name(err));
        return;
    }

    size_t cert_len = ca_pem_end - ca_pem_start;
    err = esp_tls_set_global_ca_store(ca_pem_start, cert_len);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "Error al cargar CA store: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI("OTA", "CA global cargado correctamente (%d bytes)", (int)cert_len);
    }
}
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
#include "nvs_flash.h"
#include "wifi_sta.h"

static const char *TAG = "APP_INIT";

extern const uint8_t ca_pem_start[] asm("_binary_ca_pem_start");
extern const uint8_t ca_pem_end[]   asm("_binary_ca_pem_end");

void inicializar_certificados_globales(void);

esp_err_t inicializar_componentes(void)
{
    esp_err_t ret;

    // Inicializar NVS Flash
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Borrando y reinicializando NVS...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializar NVS Manager
    ret = nvs_manager_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar NVS Manager: %s", esp_err_to_name(ret));
        return ret;
    }

    // Inicializar WiFi en modo estación
    ret = sta_wifi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    // Guardar explícitamente credenciales WiFi de Kconfig en NVS
    ret = sta_wifi_save_kconfig_to_nvs();
    if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "No se pudieron guardar credenciales WiFi de Kconfig: %s", esp_err_to_name(ret));
    }

    // Pausa más larga para asegurar que NVS esté listo
    vTaskDelay(pdMS_TO_TICKS(500));

    // Inicializar LED
    ESP_LOGI(TAG, "Inicializando LED...");
    ret = led_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al inicializar LED: %s", esp_err_to_name(ret));
        return ret;
    }
    led_blink_start(100);

    // Inicializar Relay Controller
    ESP_LOGI(TAG, "Inicializando Relay Controller...");
    ret = relay_controller_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al inicializar Relay Controller: %s", esp_err_to_name(ret));
        return ret;
    }

    // Pausa corta
    vTaskDelay(pdMS_TO_TICKS(100));

    // Inicializar el control boton (solo inicialización interna)
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
#include "estado_inicio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_control.h"
#include "nvs_manager.h"

static const char *TAG = "ESTADO_INICIO";

esp_err_t estado_inicio_iniciar(void) {
    ESP_LOGI(TAG, "Iniciando estado INICIO. Esperando 5 segundos antes de cambiar...");

    vTaskDelay(pdMS_TO_TICKS(5000));

    // Transición genérica a CONFIGURACIÓN usando nueva API sin duplicar tareas
    esp_err_t err = app_control_lanzar_transicion(ESTADO_CONFIGURACION, TAG);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al lanzar transición genérica a CONFIGURACIÓN");
        return err;
    }

    return ESP_OK;
}

esp_err_t estado_inicio_detener(void) {
    ESP_LOGI(TAG, "Finalizando estado de INICIO");
    return ESP_OK;
}

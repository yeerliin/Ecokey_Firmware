#include "estado_manual.h"
#include "esp_log.h"
#include <stdbool.h>  // Añadido para tipos bool, true, false

static const char *TAG = "ESTADO_NORMAL";
static bool estado_activo = false;

esp_err_t estado_manual_iniciar(void) {
    if (estado_activo) {
        ESP_LOGW(TAG, "Estado normal ya está activo");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Iniciando el modo normal");
    
    // Aquí iría la lógica específica del estado normal
    // Por ahora solo mostramos un log simple

    estado_activo = true;
    return ESP_OK;
}

esp_err_t estado_manual_detener(void) {
    if (!estado_activo) {
        ESP_LOGW(TAG, "Estado normal no está activo");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deteniendo el modo normal");
    
    // Aquí iría la lógica para detener el estado normal
    // Por ahora solo mostramos un log simple

    estado_activo = false;
    return ESP_OK;
}

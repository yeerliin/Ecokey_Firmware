#include "estado_manual.h"
#include "esp_log.h"
#include "relay_controller.h" // Añadido para acceder al controlador de relé


static const char *TAG = "ESTADO_MANUAL"; // Corregido el nombre del tag
static bool estado_activo = false;

esp_err_t estado_manual_iniciar(void) {
    if (estado_activo) {
        ESP_LOGW(TAG, "Estado manual ya está activo");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Iniciando el modo manual");
    
    // Aquí iría la lógica específica del estado manual
    // Por ahora solo mostramos un log simple

    estado_activo = true;
    return ESP_OK;
}

esp_err_t estado_manual_detener(void) {
    if (!estado_activo) {
        ESP_LOGW(TAG, "Estado manual no está activo");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deteniendo el modo manual");
    
    // Aquí iría la lógica para detener el estado manual
    // Por ahora solo mostramos un log simple

    estado_activo = false;
    return ESP_OK;
}

esp_err_t estado_manual_alternar_rele(void) {
    if (!estado_activo) {
        ESP_LOGW(TAG, "No se puede alternar relé: estado manual no activo");
        return ESP_ERR_INVALID_STATE;
    }
    
    bool estado_actual;
    esp_err_t ret = relay_controller_get_state(&estado_actual);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al obtener estado del relé: %s", esp_err_to_name(ret));
        return ret;
    }
    
    bool nuevo_estado = !estado_actual;
    ret = relay_controller_set_state(nuevo_estado);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Relé %s", nuevo_estado ? "ACTIVADO" : "DESACTIVADO");
        
        // Aquí podrías añadir la notificación MQTT u otras acciones
        // cuando se modifica el estado del relé
    }
    
    return ret;
}

esp_err_t estado_manual_obtener_estado_rele(bool *estado) {
    if (estado == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return relay_controller_get_state(estado);
}

bool estado_manual_esta_activo(void) {
    return estado_activo;
}

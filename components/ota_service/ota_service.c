#include "ota_service.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "mqtt_service.h"
#include "esp_crt_bundle.h"
#include <inttypes.h> // Incluir para usar PRIx32

static const char *TAG = "ota_service";

esp_err_t ota_service_start_update(const char *url, bool forzar)
{
    if (!url || strlen(url) < 8) {
        ESP_LOGE(TAG, "URL no válida para OTA: %s", url ? url : "(null)");
        return ESP_ERR_INVALID_ARG;
    }

    // Verificar primero si hay una actualización pendiente de confirmar
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    esp_err_t err = esp_ota_get_state_partition(running, &ota_state);
    
    // Si hay un firmware pendiente de verificar, confirmarlo automáticamente
    if (err == ESP_OK && ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGW(TAG, "Firmware actual en estado PENDING_VERIFY, validando automáticamente");
        esp_ota_mark_app_valid_cancel_rollback();
    }

    // Verificar que tengamos una partición OTA disponible
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No se encontró partición OTA disponible");
        mqtt_service_enviar_json("ota/status", 1, 0, "estado", "error", 
                               "mensaje", "No hay partición OTA disponible", 
                               "tipo", "respuesta", NULL);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Partición OTA disponible: %s, offset 0x%" PRIx32,
             update_partition->label, update_partition->address);

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach, // Usar el bundle de certificados integrado
        .skip_cert_common_name_check = false,
        .timeout_ms = 30000, // Aumentar el timeout para descargas lentas
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
    };

    // Verificar preventivamente la respuesta HTTP antes de iniciar la OTA
    ESP_LOGI(TAG, "Verificando URL antes de iniciar OTA: %s", url);
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t open_ret = esp_http_client_open(client, 0);
    if (open_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al abrir conexión OTA: %s", esp_err_to_name(open_ret));
    } else {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Respuesta HTTP: %d", status);
        
        // Si el status es un error (4xx o 5xx), no seguir con la actualización
        if (status >= 400) {
            ESP_LOGE(TAG, "Error en la respuesta HTTP: %d", status);
            esp_http_client_cleanup(client);
            mqtt_service_enviar_json("ota/status", 1, 0, "estado", "error", 
                                   "mensaje", "Error HTTP", "codigo", "404", 
                                   "tipo", "respuesta", NULL);
            return ESP_FAIL;
        }
    }
    esp_http_client_cleanup(client);

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    ESP_LOGI(TAG, "Iniciando OTA desde: %s", url);
    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA finalizada correctamente. Reiniciando...");
        mqtt_service_enviar_json("ota/status", 1, 0, "estado", "exito", "tipo", "respuesta", NULL);
        vTaskDelay(pdMS_TO_TICKS(1000)); // pequeña pausa
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Fallo OTA: %s", esp_err_to_name(ret));
        mqtt_service_enviar_json("ota/status", 1, 0, "estado", "error", 
                               "mensaje", "OTA fallida", 
                               "error", esp_err_to_name(ret), 
                               "tipo", "respuesta", NULL);
    }

    return ret;
}

void ota_service_verificar_rollback(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    esp_err_t err = esp_ota_get_state_partition(running, &ota_state);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al obtener estado OTA: %s", esp_err_to_name(err));
        return;
    }

    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGW(TAG, "Firmware en estado PENDING_VERIFY, validando...");

        // Aquí puedes realizar pruebas adicionales (sensor, red, etc.)
        bool firmware_funciona = true; // ← personalízalo según necesidades

        if (firmware_funciona) {
            ESP_LOGI(TAG, "Marcando firmware como válido...");
            esp_err_t val_err = esp_ota_mark_app_valid_cancel_rollback();
            if (val_err != ESP_OK) {
                ESP_LOGE(TAG, "Error al marcar firmware como válido: %s", esp_err_to_name(val_err));
            } else {
                ESP_LOGI(TAG, "Firmware marcado como válido correctamente");
            }
        } else {
            ESP_LOGE(TAG, "Fallo funcional, revirtiendo...");
            mqtt_service_enviar_json("ota/status", 1, 0, "estado", "rollback", "motivo", "verificación fallida", NULL);
            esp_ota_mark_app_invalid_rollback_and_reboot();
        }
    } else {
        ESP_LOGI(TAG, "Firmware actual ya está verificado (estado: %d)", ota_state);
    }
    
    // Imprimir información adicional sobre las particiones
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running_part = esp_ota_get_running_partition();
    const esp_partition_t *next_update = esp_ota_get_next_update_partition(NULL);
    
    ESP_LOGI(TAG, "Particiones OTA - Configurada: %s, En ejecución: %s, Próxima actualización: %s",
             configured ? configured->label : "NULL", 
             running_part ? running_part->label : "NULL",
             next_update ? next_update->label : "NULL");
}

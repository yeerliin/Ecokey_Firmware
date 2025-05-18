#pragma once

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* ap_pass;       // Password del SoftAP
    int ap_channel;            // Canal WiFi (default 1)
    int ap_max_conn;           // Máx. clientes (default 4)
    void (*on_provisioned)(void); // Callback tras provisión (opcional)
} wifi_prov_web_config_t;

/**
 * Inicia el proceso de aprovisionamiento WiFi vía web.
 * @param config Configuración del provisioning.
 * @return ESP_OK si todo fue correcto.
 */
esp_err_t wifi_provision_web_start(const wifi_prov_web_config_t *config);

/**
 * Detiene el provisioning y cierra el servidor web.
 */
void wifi_provision_web_stop(void);

/**
 * ¿Está corriendo el provisioning?
 */
bool wifi_provision_web_is_running(void);

#ifdef __cplusplus
}
#endif

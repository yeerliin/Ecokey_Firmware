#pragma once

#include "esp_err.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicia la actualización OTA desde una URL HTTPS.
 * 
 * @param url URL completa del binario del firmware
 * @param forzar si es true, permite actualizar aunque la versión sea igual
 * @return esp_err_t ESP_OK si se inicia correctamente, error en caso contrario
 */
esp_err_t ota_service_start_update(const char *url, bool forzar);

/**
 * @brief Verifica si el firmware actual está en estado PENDING_VERIFY
 *        y lo valida o revierte según condiciones internas.
 */
void ota_service_verificar_rollback(void);

#ifdef __cplusplus
}
#endif

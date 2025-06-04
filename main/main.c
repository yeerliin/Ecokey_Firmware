#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "esp_system.h"
#include "app_inicializacion.h" // Inicialización de componentes básicos
#include "app_control.h"        // Control de estados1

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando aplicación EcoKey");

    // 1. Inicializar todos los componentes básicos (incluido app_control)
    if (inicializar_componentes() != ESP_OK)
    {
        ESP_LOGE(TAG, "Error durante la inicialización de componentes");
        return; // Terminamos la ejecución si hay error crítico
    }

    ESP_LOGI(TAG, "Inicialización completada, iniciando estado de la aplicación");

    // 2. Iniciar el estado adecuado según la configuración guardada o lógica predeterminada
    if (app_control_iniciar_estado() != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al iniciar el estado de la aplicación");
        return; // Terminamos si no se puede iniciar ningún estado
    }

    ESP_LOGI(TAG, "Aplicación en ejecución con estado inicial activado");

    
}

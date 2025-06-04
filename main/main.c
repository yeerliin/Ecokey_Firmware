#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "esp_system.h"
#include "app_inicializacion.h" // Inicialización de componentes básicos
#include "app_control.h"        // Control de estados1

static const char *TAG = "MAIN";

static const char *reset_reason_to_str(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_UNKNOWN:
        return "UNKNOWN";
    case ESP_RST_POWERON:
        return "POWERON";
    case ESP_RST_EXT:
        return "EXT";
    case ESP_RST_SW:
        return "SW";
    case ESP_RST_PANIC:
        return "PANIC";
    case ESP_RST_INT_WDT:
        return "INT_WDT";
    case ESP_RST_TASK_WDT:
        return "TASK_WDT";
    case ESP_RST_WDT:
        return "WDT";
    case ESP_RST_DEEPSLEEP:
        return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
        return "BROWNOUT";
    case ESP_RST_SDIO:
        return "SDIO";
    case ESP_RST_USB:
        return "USB";
    case ESP_RST_JTAG:
        return "JTAG";
    case ESP_RST_RTCWDT:
        return "RTC_WDT";
    case ESP_RST_CORE_SW:
        return "CORE_SW";
    case ESP_RST_CORE_DEEP_SLEEP:
        return "CORE_DEEPSLEEP";
    default:
        return "UNKNOWN";
    }
}

void app_main(void)
{
    esp_reset_reason_t reset_reason = esp_reset_reason();
    ESP_LOGI(TAG, "Reset reason: %s (%d)", reset_reason_to_str(reset_reason), reset_reason);

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

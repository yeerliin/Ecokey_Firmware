#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "esp_system.h"
#include "app_inicializacion.h" // Inicialización de componentes básicos
#include "app_control.h"        // Control de estados1

static const char *TAG = "MAIN";
static const char *TAG_RESET = "RESET_INFO"; // TAG específico para información de reinicio

// Variable global para guardar el motivo de reinicio
esp_reset_reason_t motivo_reinicio_global;
const char* str_motivo_reinicio_global;

// Función para obtener la descripción del motivo de reinicio
static const char* obtener_descripcion_reset(esp_reset_reason_t motivo)
{
    switch (motivo)
    {
        case ESP_RST_UNKNOWN:
            return "Desconocido";
        case ESP_RST_POWERON:
            return "Encendido normal";
        case ESP_RST_EXT:
            return "Reset externo";
        case ESP_RST_SW:
            return "Reset por software (esp_restart)";
        case ESP_RST_PANIC:
            return "Reset por excepción/pánico";
        case ESP_RST_INT_WDT:
            return "Reset por watchdog de interrupción";
        case ESP_RST_TASK_WDT:
            return "Reset por watchdog de tarea";
        case ESP_RST_WDT:
            return "Reset por otros watchdogs";
        case ESP_RST_DEEPSLEEP:
            return "Reset después de salir de deep sleep";
        case ESP_RST_BROWNOUT:
            return "Reset por brownout";
        case ESP_RST_SDIO:
            return "Reset por SDIO";
        case ESP_RST_USB:
            return "Reset por periférico USB";
        case ESP_RST_JTAG:
            return "Reset por JTAG";
        case ESP_RST_EFUSE:
            return "Reset debido a error de eFuse";
        case ESP_RST_PWR_GLITCH:
            return "Reset debido a glitch de energía detectado";
        case ESP_RST_CPU_LOCKUP:
            return "Reset debido a bloqueo de CPU (doble excepción)";
        default:
            return "Motivo desconocido";
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando aplicación EcoKey");

    // Obtener y mostrar el motivo del último reinicio
    motivo_reinicio_global = esp_reset_reason();
    str_motivo_reinicio_global = obtener_descripcion_reset(motivo_reinicio_global);
    
    // Usar ESP_LOGW para que aparezca en color amarillo
    ESP_LOGW(TAG_RESET, "╔══════════════════════════════════════╗");
    ESP_LOGW(TAG_RESET, "║  MOTIVO DEL ÚLTIMO REINICIO: %-8s ║", str_motivo_reinicio_global);
    ESP_LOGW(TAG_RESET, "╚══════════════════════════════════════╝");

    vTaskDelay(pdMS_TO_TICKS(1000)); // Pausa para estabilidad
    
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

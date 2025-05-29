#include "estado_automatico.h"
#include "esp_log.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "ble_scanner.h"
#include "relay_controller.h"
#include "nvs_manager.h"
#include <string.h>
#include "mqtt_service.h"
#include "wifi_sta.h"
#include "time_manager.h"

static const char *TAG = "ESTADO_AUTO";
static bool estado_activo = false;
static TaskHandle_t automatico_task_handle = NULL;
static volatile uint32_t automatico_timeout_ms = 10 * 60 * 1000; // 10 minutos por defecto

// Configuración
#define AUTOMATICO_TASK_PERIOD_MS 500
#define BLE_TARGET_INDEX 0 // Usar el índice 0 para la MAC objetivo

static void automatico_task(void *param)
{
    int64_t last_detected_time = 0;
    bool rele_activado = false;

    while (estado_activo)
    {
        bool detectado = ble_scanner_tag_detectado(BLE_TARGET_INDEX);
        int64_t now = esp_timer_get_time() / 1000; // ms

        uint32_t timeout_ms = automatico_timeout_ms; // leer valor actual

        if (detectado)
        {
            last_detected_time = now;
            if (!rele_activado)
            {
                relay_controller_activate();
                rele_activado = true;
                ESP_LOGI(TAG, "Relé activado por detección BLE");
            }
        }
        else
        {
            if (rele_activado && (now - last_detected_time > timeout_ms))
            {
                relay_controller_deactivate();
                rele_activado = false;
                ESP_LOGI(TAG, "Relé desactivado por ausencia de BLE >%lu ms", timeout_ms);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(AUTOMATICO_TASK_PERIOD_MS));
    }

    // Al salir, asegurar relé desactivado
    relay_controller_deactivate();
    ESP_LOGI(TAG, "Tarea automática detenida y relé desactivado");

    // Limpieza segura del handle aquí
    automatico_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t estado_automatico_iniciar(void)
{
    if (estado_activo)
    {
        ESP_LOGW(TAG, "Estado automático ya está activo");
        return ESP_OK;
    }
    else
    {
        const char *mac = sta_wifi_get_mac_str();         // MAC con dos puntos para el JSON
        const char *mac_topic = sta_wifi_get_mac_clean(); // MAC sin dos puntos para el topic
        char topic[64];
        snprintf(topic, sizeof(topic), "dispositivos/%s", mac_topic);
        char fecha_actual[24];
        if (time_manager_get_fecha_actual(fecha_actual, sizeof(fecha_actual)) == ESP_OK)
        {
            mqtt_service_enviar_json(topic, 2, 1, "Modo", "automatico", "Fecha", fecha_actual, NULL);
            printf("Fecha actual: %s\n", fecha_actual);
        }
        else
        {
            mqtt_service_enviar_json(topic, 2, 1, "Modo", "automaticoo", NULL);
        }
    }

    ESP_LOGI(TAG, "Iniciando el modo automático");

    // Recuperar la MAC objetivo desde NVS
    char mac_objetivo[18] = {0}; // Formato XX:XX:XX:XX:XX:XX (17 chars + null)
    esp_err_t err = nvs_manager_get_string("mac_objetivo", mac_objetivo, sizeof(mac_objetivo));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al recuperar MAC objetivo de NVS: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
    if (strlen(mac_objetivo) < 12)
    {
        ESP_LOGE(TAG, "MAC objetivo no válida: %s", mac_objetivo);
        return ESP_FAIL;
    }
    // Configurar la MAC en el escáner BLE
    ESP_LOGI(TAG, "Configurando escáner BLE para MAC objetivo: %s", mac_objetivo);
    err = ble_scanner_configurar_mac_objetivo_texto(BLE_TARGET_INDEX, mac_objetivo);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al configurar MAC objetivo en escáner BLE: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
    // Iniciar el escáner BLE si no está iniciado
    err = ble_scanner_iniciar(NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al iniciar escáner BLE: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // Recuperar el temporizador desde NVS (en minutos)
    char temporizador_str[8] = {0};
    int minutos = 10; // valor por defecto
    err = nvs_manager_get_string("temporizador", temporizador_str, sizeof(temporizador_str));
    if (err == ESP_OK)
    {
        int temp = atoi(temporizador_str);
        if (temp >= 1 && temp <= 30)
        {
            minutos = temp;
        }
        else
        {
            ESP_LOGW(TAG, "Valor de temporizador fuera de rango (%d), usando 10 min por defecto", temp);
        }
    }
    else
    {
        ESP_LOGW(TAG, "No se pudo recuperar temporizador, usando 10 min por defecto");
    }
    uint32_t timeout_ms = (uint32_t)minutos * 60 * 1000; // convertir minutos a ms
    automatico_timeout_ms = timeout_ms;

    ESP_LOGI(TAG, "Timeout de ausencia BLE configurado en %d minutos (%lu ms)", minutos, timeout_ms);

    estado_activo = true;

    if (automatico_task_handle == NULL)
    {
        BaseType_t res = xTaskCreate(
            automatico_task, "automatico_task", 4096, NULL, 5, &automatico_task_handle);
        if (res != pdPASS)
        {
            ESP_LOGE(TAG, "Error al crear la tarea automática");
            estado_activo = false;
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t estado_automatico_detener(void)
{
    if (!estado_activo)
    {
        ESP_LOGW(TAG, "Estado automático no está activo");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deteniendo el modo automático");
    estado_activo = false;

    // Esperar a que la tarea termine y libere el handle
    while (automatico_task_handle != NULL)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    relay_controller_set_state(false);
    // Opcional: detener BLE y limpiar si aplica
    ble_scanner_deinicializar();

    return ESP_OK;
}

// (Opcional) Función para actualizar el timeout desde otro componente (ej: MQTT)
void estado_automatico_set_timeout_minutos(uint32_t minutos)
{
    if (minutos < 1)
        minutos = 1;
    if (minutos > 30)
        minutos = 30;
    automatico_timeout_ms = minutos * 60 * 1000;
    ESP_LOGI(TAG, "Timeout actualizado dinámicamente a %lu minutos (%lu ms)", minutos, automatico_timeout_ms);

    // Guardar en NVS para persistencia tras reinicio
    char temp_str[8];
    snprintf(temp_str, sizeof(temp_str), "%lu", minutos);
    esp_err_t err = nvs_manager_set_string("temporizador", temp_str);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "No se pudo guardar el nuevo timeout en NVS: %s", esp_err_to_name(err));
    }
}

uint32_t estado_automatico_get_timeout_minutos(void)
{
    return automatico_timeout_ms / 60000;
}

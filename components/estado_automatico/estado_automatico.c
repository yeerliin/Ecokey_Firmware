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
#include "led.h" // Para detener el parpadeo del LED al iniciar el modo automático

static const char *TAG = "ESTADO_AUTO";
static bool estado_activo = false;
static TaskHandle_t automatico_task_handle = NULL;
static volatile uint32_t automatico_timeout_ms = 10 * 60 * 1000; // 10 minutos por defecto

// Configuración
#define AUTOMATICO_TASK_PERIOD_MS 500
#define BLE_TARGET_INDEX 0 // Usar el índice 0 para la MAC objetivo

// Parámetros de ventana de re-chequeo
#define FRACCION_RECHEQUEO 4         // 1/4 del tiempo total
#define MIN_RECHEQUEO_MS (50 * 1000) // Nunca menos de 50 segundos

static void automatico_task(void *param)
{
    int64_t last_detected_time = 0;
    bool rele_activado = false;
    bool escaneo_activo = false;
    int64_t rele_apagado_time = 0;
    uint32_t timeout_ms = automatico_timeout_ms;
    uint32_t ventana_rechequeo_ms = timeout_ms / FRACCION_RECHEQUEO;
    if (ventana_rechequeo_ms < MIN_RECHEQUEO_MS) ventana_rechequeo_ms = MIN_RECHEQUEO_MS;

    // Nuevo: contador para reinicio periódico del escáner
    const int64_t REINICIO_ESCANEO_MS = 30 * 60 * 1000; // 30 minutos
    int64_t ultimo_reinicio_escaneo = esp_timer_get_time() / 1000;

    while (estado_activo)
    {
        timeout_ms = automatico_timeout_ms; // Por si cambia en caliente
        ventana_rechequeo_ms = timeout_ms / FRACCION_RECHEQUEO;
        if (ventana_rechequeo_ms < MIN_RECHEQUEO_MS) ventana_rechequeo_ms = MIN_RECHEQUEO_MS;

        int64_t now = esp_timer_get_time() / 1000; // ms

        if (!rele_activado) {
            // Relé apagado: escaneo BLE siempre activo
            if (!escaneo_activo) {
                ble_scanner_reiniciar();
                escaneo_activo = true;
                ESP_LOGI(TAG, "Escaneo BLE activado (relé apagado)");
            }

            bool detectado = ble_scanner_tag_detectado(BLE_TARGET_INDEX);
            if (detectado) {
                last_detected_time = now;
                relay_controller_activate();
                rele_activado = true;
                ESP_LOGI(TAG, "Relé activado por detección BLE");
                ble_scanner_detener();
                escaneo_activo = false;
                rele_apagado_time = now + timeout_ms;
            } else {
                // Si lleva ausente más de 1 minuto, aplicar duty cycle
                if ((now - last_detected_time) > 60000) {
                    // Duty cycle: escanear 5s, pausar 5s
                    if (escaneo_activo && ((now / 5000) % 2 == 1)) {
                        ble_scanner_detener();
                        escaneo_activo = false;
                    } else if (!escaneo_activo && ((now / 5000) % 2 == 0)) {
                        ble_scanner_reiniciar();
                        escaneo_activo = true;
                    }
                }
            }

            // Nuevo: reiniciar escáner si lleva mucho tiempo ausente
            if (escaneo_activo && (now - ultimo_reinicio_escaneo) > REINICIO_ESCANEO_MS) {
                ESP_LOGI(TAG, "Reiniciando escáner BLE por periodo largo de ausencia");
                ble_scanner_reiniciar();
                ultimo_reinicio_escaneo = now;
            }
        } else {
            // Relé encendido: escaneo BLE parado
            int64_t tiempo_restante = rele_apagado_time - now;
            bool detectado = false; // Declarar aquí para todo el bloque

            // Reactivar escaneo solo en la ventana de re-chequeo
            if (tiempo_restante <= ventana_rechequeo_ms && !escaneo_activo) {
                ble_scanner_reiniciar();
                escaneo_activo = true;
                ESP_LOGI(TAG, "Escaneo BLE reactivado (ventana de re-chequeo: %lu ms)", ventana_rechequeo_ms);
            }

            if (escaneo_activo) {
                detectado = ble_scanner_tag_detectado(BLE_TARGET_INDEX);
                if (detectado) {
                    last_detected_time = now;
                    rele_apagado_time = now + timeout_ms;
                    ble_scanner_detener();
                    escaneo_activo = false;
                    ESP_LOGI(TAG, "Tag detectado de nuevo, temporizador reiniciado y escaneo parado");
                }
            }

            if (now >= rele_apagado_time) {
                relay_controller_deactivate();
                rele_activado = false;
                ESP_LOGI(TAG, "Relé desactivado por timeout");
                // BLE escaneo se reactivará en el siguiente ciclo
            }

            // Al detectar el tag, reiniciar el contador de reinicio
            if (detectado) {
                ultimo_reinicio_escaneo = now;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(AUTOMATICO_TASK_PERIOD_MS));
    }

    // Al salir, asegurar relé desactivado y BLE parado
    relay_controller_deactivate();
    ble_scanner_deinicializar();
    ESP_LOGI(TAG, "Tarea automática detenida y relé desactivado");
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
        const char *mac_topic = sta_wifi_get_mac_clean(); // MAC sin dos puntos para el topic
        char topic[64];
        snprintf(topic, sizeof(topic), "dispositivos/%s/modo", mac_topic); // <-- Cambiado a /modo
        char fecha_actual[24];
        if (time_manager_get_fecha_actual(fecha_actual, sizeof(fecha_actual)) == ESP_OK)
        {
            mqtt_service_enviar_json(topic, 2, 1, "Modo", "automatico", "FechaModo", fecha_actual, NULL);
            printf("Fecha actual: %s\n", fecha_actual);
        }
        else
        {
            mqtt_service_enviar_json(topic, 2, 1, "Modo", "automaticoo", NULL);
        }
    }
    led_blink_stop(); 
    ESP_LOGI(TAG, "Iniciando el modo automático");

    // Recuperar la MAC objetivo desde NVS
    char mac_objetivo[24] = {0}; // Formato XX:XX:XX:XX:XX:XX (17 chars + null + extra)
    esp_err_t err = nvs_manager_get_string("mac_objetivo", mac_objetivo, sizeof(mac_objetivo));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al recuperar MAC objetivo de NVS: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "MAC objetivo cargada de NVS: [%s]", mac_objetivo);
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

// Modificar la función estado_automatico_set_timeout_minutos para actualizar la ventana de re-chequeo
void estado_automatico_set_timeout_minutos(uint32_t minutos)
{
    if (minutos < 1)
        minutos = 1;
    if (minutos > 30)
        minutos = 30;
    automatico_timeout_ms = minutos * 60 * 1000;
    ESP_LOGI(TAG, "Timeout actualizado dinámicamente a %lu minutos (%lu ms)", minutos, automatico_timeout_ms);
    
    // Calcular y mostrar la nueva ventana de re-chequeo
    uint32_t ventana_rechequeo_ms = automatico_timeout_ms / FRACCION_RECHEQUEO;
    if (ventana_rechequeo_ms < MIN_RECHEQUEO_MS) ventana_rechequeo_ms = MIN_RECHEQUEO_MS;
    ESP_LOGI(TAG, "Nueva ventana de re-chequeo: %lu ms", ventana_rechequeo_ms);

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

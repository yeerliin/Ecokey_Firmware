#include "relay_controller.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "mqtt_service.h"
#include "wifi_sta.h"
#include "time_manager.h"
#include "string.h"

static const char *TAG = "RELAY_CONTROLLER";

// Configuración fija para el relé
#define RELAY_GPIO_PIN 7     // Pin fijo para el relé
#define RELAY_ACTIVE_HIGH true // Siempre activo en nivel alto

static bool relay_state = false;
static bool relay_initialized = false;

static void enviar_estado_actual_y_historico(const char *mac_topic, const char *estado_str)
{
    char fecha_actual[24] = {0};
    esp_err_t fecha_ok = time_manager_get_fecha_actual(fecha_actual, sizeof(fecha_actual));

    // Tópico de estado actual
    char topic_estado[64];
    snprintf(topic_estado, sizeof(topic_estado), "dispositivos/%s/estado", mac_topic);

    if (fecha_ok == ESP_OK && strlen(fecha_actual) > 0) {
        mqtt_service_enviar_json(topic_estado, 2, 1, "Estado", estado_str, "Fecha", fecha_actual, NULL);
    } else {
        mqtt_service_enviar_json(topic_estado, 2, 1, "Estado", estado_str, NULL);
    }

    // Tópico histórico con fecha (solo si hay fecha)
    if (fecha_ok == ESP_OK && strlen(fecha_actual) > 0) {
        char fecha_formateada[24] = {0};
        const char *src = fecha_actual;
        char *dst = fecha_formateada;
        while (*src && (dst - fecha_formateada) < (int)sizeof(fecha_formateada) - 1) {
            if (*src == ' ' || *src == ':') *dst++ = '_';
            else if (*src == '-' || (*src >= '0' && *src <= '9')) *dst++ = *src;
            src++;
        }
        *dst = '\0';

        char topic_historico[96];
        snprintf(topic_historico, sizeof(topic_historico), "dispositivos/%s/historial/%s", mac_topic, fecha_formateada);
        mqtt_service_enviar_json(topic_historico, 2, 1, "Estado", estado_str, "Fecha", fecha_actual, NULL);
    }
}

esp_err_t relay_controller_init(void)
{
    // Evitar inicialización múltiple
    if (relay_initialized)
    {
        return ESP_OK;
    }

    // Configurar el pin GPIO como salida
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RELAY_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al configurar GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    // === POLÍTICA DE INICIALIZACIÓN SEGURA ===
    // Siempre iniciar con el relé APAGADO tras un reinicio
    // Cada modo de la aplicación decidirá si necesita encenderlo
    relay_state = false;
    gpio_set_level(RELAY_GPIO_PIN, (RELAY_ACTIVE_HIGH && relay_state) ? 1 : 0);

    relay_initialized = true;
    ESP_LOGI(TAG, "Relay controller inicializado en GPIO %d, activo en %s, estado inicial: APAGADO (seguro)",
             RELAY_GPIO_PIN, RELAY_ACTIVE_HIGH ? "ALTO" : "BAJO");

    return ESP_OK;
}

esp_err_t relay_controller_activate(void)
{
    if (!relay_initialized)
    {
        ESP_LOGE(TAG, "El controlador del relé no ha sido inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    if (!relay_state)
    {
        gpio_set_level(RELAY_GPIO_PIN, RELAY_ACTIVE_HIGH ? 1 : 0);
        relay_state = true;
        
        const char *mac_topic = sta_wifi_get_mac_clean();
        enviar_estado_actual_y_historico(mac_topic, "Encendido");
        ESP_LOGI(TAG, "Relé activado");
    }

    return ESP_OK;
}

esp_err_t relay_controller_deactivate(void)
{
    if (!relay_initialized)
    {
        ESP_LOGE(TAG, "El controlador del relé no ha sido inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    if (relay_state)
    {
        gpio_set_level(RELAY_GPIO_PIN, RELAY_ACTIVE_HIGH ? 0 : 1);
        relay_state = false;
        
        const char *mac_topic = sta_wifi_get_mac_clean();
        enviar_estado_actual_y_historico(mac_topic, "Apagado");
        ESP_LOGI(TAG, "Relé desactivado");
    }

    return ESP_OK;
}

esp_err_t relay_controller_set_state(bool state)
{
    if (state)
    {
        return relay_controller_activate();
    }
    else
    {
        return relay_controller_deactivate();
    }
}

esp_err_t relay_controller_get_state(bool *state)
{
    if (!relay_initialized)
    {
        ESP_LOGE(TAG, "El controlador del relé no ha sido inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    if (state == NULL)
    {
        ESP_LOGE(TAG, "Puntero de estado no válido (NULL)");
        return ESP_ERR_INVALID_ARG;
    }

    *state = relay_state;
    return ESP_OK;
}

// Función de pulso para activar el relé temporalmente
esp_err_t relay_controller_pulse(uint32_t duration_ms)
{
    if (!relay_initialized)
    {
        ESP_LOGE(TAG, "El controlador del relé no ha sido inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    // Activar el relé
    esp_err_t ret = relay_controller_activate();
    if (ret != ESP_OK)
    {
        return ret;
    }

    // Esperar la duración especificada
    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    // Desactivar el relé
    return relay_controller_deactivate();
}
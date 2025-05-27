#include "relay_controller.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "mqtt_service.h"
#include "wifi_sta.h"

static const char *TAG = "RELAY_CONTROLLER";

// Configuración fija para el relé
<<<<<<< HEAD
#define RELAY_GPIO_PIN 18         // Pin fijo para el relé
#define RELAY_ACTIVE_HIGH true    // Siempre activo en nivel alto
=======
#define RELAY_GPIO_PIN 7        // Pin fijo para el relé
#define RELAY_ACTIVE_HIGH false // Siempre activo en nivel alto
>>>>>>> CorregirWifi

static bool relay_state = false;
static bool relay_initialized = false;

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

    // Inicialmente desactivar el relé
    relay_state = false;
    gpio_set_level(RELAY_GPIO_PIN, RELAY_ACTIVE_HIGH ? 0 : 1);

    relay_initialized = true;
    ESP_LOGI(TAG, "Relay controller inicializado en GPIO %d, activo en %s",
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
        const char *mac = sta_wifi_get_mac_str();         // MAC con dos puntos para el JSON
        const char *mac_topic = sta_wifi_get_mac_clean(); // MAC sin dos puntos para el topic
        char topic[64];
        snprintf(topic, sizeof(topic), "dispositivos/%s", mac_topic);
        mqtt_service_enviar_json(topic, 2, 1, "MAC", mac, "Estado", "Encendido", NULL);
        // Evento crítico: activación
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
        const char *mac = sta_wifi_get_mac_str();         // MAC con dos puntos para el JSON
        const char *mac_topic = sta_wifi_get_mac_clean(); // MAC sin dos puntos para el topic
        char topic[64];
        snprintf(topic, sizeof(topic), "dispositivos/%s", mac_topic);
        mqtt_service_enviar_json(topic, 2, 1, "MAC", mac, "Estado", "Apagado", NULL);
        // Evento crítico: desactivación
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
// ...existing code...
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
        const char *mac = sta_wifi_get_mac_str();
        const char *mac_topic = sta_wifi_get_mac_clean();
        char topic[64];
        snprintf(topic, sizeof(topic), "dispositivos/%s", mac_topic);
        char fecha_actual[24];
        if (time_manager_get_fecha_actual(fecha_actual, sizeof(fecha_actual)) == ESP_OK)
        {
            mqtt_service_enviar_json(topic, 3, 1, "Estado", "Encendido", "Fecha", fecha_actual, NULL);
            printf("Fecha actual: %s\n", fecha_actual);
            // Ejemplo de salida: Fecha actual: 2024-06-10 15:23:45
        }
        else
        {
            mqtt_service_enviar_json(topic, 2, 1, "Estado", "Encendido", NULL);
        }
        // Evento crítico: activación
    }

    return ESP_OK;
}
// ...existing code...

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Inicializa y arranca el cliente MQTT.
     *
     * Debe llamarse una vez que la red esté disponible.
     * No recibe parámetros.
     */
    void mqtt_service_start(void);

    /**
     * @brief Detiene el cliente MQTT y libera los recursos asociados.
     *
     * Puede llamarse cuando ya no se necesita la conexión MQTT o antes de reiniciar el servicio.
     * No recibe parámetros.
     */
    void mqtt_service_stop(void);

    /**
     * @brief Publica un mensaje simple en un tópico MQTT.
     *
     * @param topic  Cadena con el nombre del tópico al que se enviará el mensaje.
     * @param valor  Cadena con el contenido del mensaje a publicar.
     * @param qos    Nivel de calidad de servicio (0, 1 o 2)
     * @param retain Indicador de retención (0: no retener, 1: retener)
     */
    void mqtt_service_enviar_dato(const char *topic, const char *valor, int qos, int retain);

    /**
     * @brief Se suscribe a un tópico MQTT con el QoS indicado.
     *
     * @param topic  Cadena con el nombre del tópico al que suscribirse.
     * @param qos    Entero indicando el nivel de calidad de servicio (0, 1 o 2).
     * @return       ID de la suscripción (positivo) o -1 si hay error.
     */
    int mqtt_service_suscribirse(const char *topic, int qos);

    /**
     * @brief Publica un mensaje JSON en un tópico MQTT con cualquier cantidad de pares clave-valor.
     *
     * @param topic   Tópico MQTT.
     * @param qos     Nivel de calidad de servicio (0, 1 o 2).
     * @param retain  1 para retener el mensaje en el broker, 0 para no retener.
     * @param ...     Lista de pares clave, valor (const char *), terminada en NULL.
     *
     * Ejemplo de uso:
     *   mqtt_service_enviar_json("dispositivos/nuevo", 1, 1, "mac", mac_str, NULL);
     *   mqtt_service_enviar_json("dispositivos/nuevo", 1, 1, "mac", mac_str, "fecha", "16:58h", NULL);
     *   mqtt_service_enviar_json("dispositivos/nuevo", 1, 1, "mac", mac_str, "fecha", "16:58h", "temperatura", "12", NULL);
     *   SIEMPRE TERMINAR CON NULL PARA INDICAR EL FINAL DE LA LISTA.
     */
    void mqtt_service_enviar_json(const char *topic, int qos, int retain, ...);

    /**
     * @brief Obtiene si el cliente MQTT está conectado actualmente
     * 
     * @return true si está conectado, false en caso contrario
     */
    bool mqtt_service_esta_conectado(void);

    /**
     * @brief Convierte el enum esp_reset_reason_t a string
     * 
     * @param reason El motivo del reinicio
     * @return const char* Representación en string del código
     */
    const char* esp_reset_reason_to_str(esp_reset_reason_t reason);

#ifdef __cplusplus
}
#endif

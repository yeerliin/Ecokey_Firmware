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
     */
    void mqtt_service_enviar_dato(const char *topic, const char *valor);

    /**
     * @brief Se suscribe a un tópico MQTT con el QoS indicado.
     *
     * @param topic  Cadena con el nombre del tópico al que suscribirse.
     * @param qos    Entero indicando el nivel de calidad de servicio (0, 1 o 2).
     * @return       ID de la suscripción (positivo) o -1 si hay error.
     */
    int mqtt_service_suscribirse(const char *topic, int qos);

    /**
     * @brief Publica un mensaje JSON con dos pares clave-valor en un tópico MQTT.
     *
     * El mensaje tendrá el formato: {"clave1":"valor1","clave2":"valor2"}
     *
     * @param topic   Cadena con el nombre del tópico al que se enviará el mensaje.
     * @param clave1  Cadena con el nombre de la primera clave del JSON.
     * @param valor1  Cadena con el valor asociado a la primera clave.
     * @param clave2  Cadena con el nombre de la segunda clave del JSON.
     * @param valor2  Cadena con el valor asociado a la segunda clave.
     */
    void mqtt_service_enviar_json(const char *topic, const char *clave1, const char *valor1,
                                  const char *clave2, const char *valor2);

    /**
     * @brief Publica un mensaje JSON arbitrario en un tópico MQTT (Para crear el Json dinamico, sin saber cuantos datos enviar.).
     *  Primero se crea el JSON con snprintf o similar, luego se envía.
     * @param topic     Cadena con el nombre del tópico al que se enviará el mensaje.
     * @param json_str  Cadena con el JSON ya formateado.
     */
    void mqtt_service_enviar_json_str(const char *topic, const char *json_str);

    /*EJEMPLO:
    char json[256];
    snprintf(json, sizeof(json),
        "{\"estado\":\"manual\",\"mac\":\"AA:BB:AA:AA:AA:AA\",\"luz\":\"encendido\",\"fecha\":\"17:00h 23/02/2025\"}");
    mqtt_service_enviar_json_str("topic/casa", json);
    */

#ifdef __cplusplus
}
#endif

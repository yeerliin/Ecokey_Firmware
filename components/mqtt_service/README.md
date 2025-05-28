
# Componente `mqtt_service`

Este componente gestiona la conexión MQTT, la publicación y suscripción de tópicos, y el procesamiento de mensajes para la configuración y control remoto del dispositivo.

## Publicaciones MQTT

- **dispositivos/nuevo**
  - Publica un JSON al conectarse con información del dispositivo:
    - Ejemplo:  
      ```json
      {"Mac": "AA:BB:CC:DD:EE:FF", "fecha": "12h"}
      ```

- **dispositivos/<mac_sin_dos_puntos>**
  - Publica el estado del relé cada vez que se activa o desactiva, incluyendo la fecha si está disponible.
    - Ejemplo al activar:
      ```json
      {"Estado": "Encendido", "Fecha": "2024-06-10 15:23:45"}
      ```
    - Ejemplo al desactivar:
      ```json
      {"Estado": "Apagado", "Fecha": "2024-06-10 15:23:45"}
      ```

## Suscripciones MQTT

- **Dispositivo/<mac_con_dos_puntos>**
  - El dispositivo se suscribe a este tópico al conectarse.
  - Espera recibir mensajes JSON para configuración remota.
  - Campos procesados:
    - `"macObjetivo"`: Actualiza la MAC objetivo para el escaneo BLE.
    - `"temporizador"`: Actualiza el temporizador de ausencia BLE (en minutos).
    - `"Estado"`: Controla el relé de forma remota y fuerza el modo manual.

  - Ejemplo de mensaje recibido:
    ```json
    {
      "macObjetivo": "AA:BB:CC:DD:EE:FF",
      "temporizador": 5,
      "Estado": true
    }
    ```
    - Todos los campos son opcionales y pueden enviarse individualmente.

## Resumen de comportamiento

- Al recibir `"Estado"` por MQTT:
  - El relé se activa o desactiva según el valor.
  - El dispositivo cambia a modo manual si no lo estaba.
  - El estado se guarda en NVS para persistencia tras reinicio.

- Al recibir `"macObjetivo"` o `"temporizador"`:
  - Se actualizan en NVS y se aplican dinámicamente al escaneo BLE.

## Notas

- El componente es idempotente: múltiples llamadas a `mqtt_service_start()` o `mqtt_service_stop()` no causan problemas.
- El JSON publicado y recibido debe estar bien formado.
- El control remoto (por MQTT) tiene prioridad y fuerza el modo manual.

---

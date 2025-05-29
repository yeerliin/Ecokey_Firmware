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

- **ota/<mac_sin_dos_puntos>**
  - Publica respuestas a las solicitudes de actualización OTA:
    - Ejemplo:
      ```json
      {"estado": "iniciando", "version": "1.2.0"}
      ```
    - Estados posibles: "iniciando", "descargando", "verificando", "instalando", "completado", "error"

## Suscripciones MQTT

- **dispositivos/<mac_sin_dos_puntos>**
  - El dispositivo se suscribe a este tópico al conectarse.
  - Espera recibir mensajes JSON para configuración remota.
  - Campos procesados:
    - `"macObjetivo"`: Actualiza la MAC objetivo para el escaneo BLE.
    - `"temporizador"`: Actualiza el temporizador de ausencia BLE (en minutos).
    - `"Estado"`: Controla el relé de forma remota y fuerza el modo manual.
    - `"Modo"`: Cambia el modo de operación entre "manual" o "automatico".

  - Ejemplo de mensaje recibido:
    ```json
    {
      "macObjetivo": "AA:BB:CC:DD:EE:FF",
      "temporizador": 5,
      "Estado": true,
      "Modo": "manual"
    }
    ```
    - Todos los campos son opcionales y pueden enviarse individualmente.

- **ota/<mac_sin_dos_puntos>**
  - El dispositivo se suscribe a este tópico para recibir actualizaciones OTA.
  - Espera un JSON con información para actualización:
    ```json
    {
      "url": "https://servidor.com/firmware.bin",
      "version": "1.2.0",
      "force": false
    }
    ```
    - `url`: URL del archivo de firmware (obligatoria)
    - `version`: Versión del nuevo firmware (opcional)
    - `force`: Si debe forzar la actualización incluso si la versión es la misma (opcional, por defecto false)

## Resumen de comportamiento

- Al recibir `"Estado"` por MQTT:
  - El relé se activa o desactiva según el valor.
  - El dispositivo cambia a modo manual si no lo estaba.
  - El estado se guarda en NVS para persistencia tras reinicio.

- Al recibir `"Modo"` por MQTT:
  - Se cambia el modo de operación según el valor ("manual" o "automatico").
  - El cambio se aplica inmediatamente.

- Al recibir `"macObjetivo"` o `"temporizador"`:
  - Se actualizan en NVS y se aplican dinámicamente al escaneo BLE.

- Al recibir mensaje en tópico OTA:
  - Se inicia el proceso de descarga y actualización del firmware.
  - Se notifica el progreso mediante publicaciones en el mismo tópico.

## Notas

- El componente es idempotente: múltiples llamadas a `mqtt_service_start()` o `mqtt_service_stop()` no causan problemas.
- El JSON publicado y recibido debe estar bien formado.
- El control remoto (por MQTT) tiene prioridad y fuerza el modo manual.
- Las actualizaciones OTA deben apuntar a un firmware válido compatible con el dispositivo.

---

| Dispositivos Compatibles | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ------------------------ | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- |

# Aplicación de ejemplo ESP-MQTT SSL

(Consulte el archivo README.md en el directorio de nivel superior 'examples' para más información sobre los ejemplos.)

Este ejemplo se conecta al broker mqtt.eclipseprojects.io usando transporte ssl y como demostración se suscribe/desuscribe y envía un mensaje en un tema determinado.
(Tenga en cuenta que el broker público es mantenido por la comunidad, por lo que puede no estar siempre disponible, para más detalles consulte este [aviso legal](https://iot.eclipse.org/getting-started/#sandboxes))

Utiliza la biblioteca ESP-MQTT que implementa un cliente mqtt para conectarse a un broker mqtt.

## Cómo usar el ejemplo

### Hardware Requerido

Este ejemplo puede ejecutarse en cualquier placa ESP32, la única interfaz requerida es WiFi y conexión a Internet.

### Configurar el proyecto

* Abra el menú de configuración del proyecto (`idf.py menuconfig`)
* Configure Wi-Fi o Ethernet en el menú "Example Connection Configuration". Consulte la sección "Estableciendo conexión Wi-Fi o Ethernet" en [examples/protocols/README.md](../../README.md) para más detalles.

El certificado PEM para este ejemplo podría extraerse de un comando `s_client` de openssl conectándose a mqtt.eclipseprojects.io.
En caso de que el sistema operativo host tenga instalados los paquetes `openssl` y `sed`, se podría ejecutar el siguiente comando para descargar y guardar el certificado raíz en un archivo (Nota para usuarios de Windows: Se pueden usar tanto entornos tipo Linux como paquetes nativos de Windows).
```
echo "" | openssl s_client -showcerts -connect mqtt.eclipseprojects.io:8883 | sed -n "1,/Root/d; /BEGIN/,/END/p" | openssl x509 -outform PEM >mqtt_eclipse_org.pem
```
Tenga en cuenta que este no es un comando general para descargar un certificado raíz para un host arbitrario;
este comando funciona con mqtt.eclipseprojects.io ya que el sitio proporciona el certificado raíz en la cadena, que luego podría extraerse
con operaciones de texto.

### Compilar y Flashear

Compile el proyecto y flashéelo en la placa, luego ejecute la herramienta de monitoreo para ver la salida serial:

```
idf.py -p PUERTO flash monitor
```

(Para salir del monitor serial, escriba ``Ctrl-]``.)

Consulte la Guía de Inicio para conocer los pasos completos para configurar y usar ESP-IDF para construir proyectos.

## Salida de ejemplo

```
I (3714) event: sta ip: 192.168.0.139, mask: 255.255.255.0, gw: 192.168.0.2
I (3714) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (3964) MQTT_CLIENT: Sending MQTT CONNECT message, type: 1, id: 0000
I (4164) MQTTS_EXAMPLE: MQTT_EVENT_CONNECTED
I (4174) MQTTS_EXAMPLE: sent publish successful, msg_id=41464
I (4174) MQTTS_EXAMPLE: sent subscribe successful, msg_id=17886
I (4174) MQTTS_EXAMPLE: sent subscribe successful, msg_id=42970
I (4184) MQTTS_EXAMPLE: sent unsubscribe successful, msg_id=50241
I (4314) MQTTS_EXAMPLE: MQTT_EVENT_PUBLISHED, msg_id=41464
I (4484) MQTTS_EXAMPLE: MQTT_EVENT_SUBSCRIBED, msg_id=17886
I (4484) MQTTS_EXAMPLE: sent publish successful, msg_id=0
I (4684) MQTTS_EXAMPLE: MQTT_EVENT_SUBSCRIBED, msg_id=42970
I (4684) MQTTS_EXAMPLE: sent publish successful, msg_id=0
I (4884) MQTT_CLIENT: deliver_publish, message_length_read=19, message_length=19
I (4884) MQTTS_EXAMPLE: MQTT_EVENT_DATA
TOPIC=/topic/qos0
DATA=data
I (5194) MQTT_CLIENT: deliver_publish, message_length_read=19, message_length=19
I (5194) MQTTS_EXAMPLE: MQTT_EVENT_DATA
TOPIC=/topic/qos0
DATA=data
```


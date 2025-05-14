| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- |

# Ejemplo de Sensor de Proximidad BLE Periférico

(Consulta el archivo README.md en el directorio de 'examples' de nivel superior para obtener más información sobre los ejemplos).

Este ejemplo crea un servidor GATT que demuestra el perfil estándar de Sensor de Proximidad. Notifica a la aplicación cuando la distancia entre dispositivos aumenta.

Anuncia soporte para el servicio de Pérdida de Enlace (Link Loss service, 0x1803) como UUID de servicio primario.

La tarea `ble_prox_prph_task` comienza a alertar a la aplicación tan pronto como se recibe un comando de escritura del dispositivo central relacionado con la pérdida de ruta que excede el umbral. La alerta se detiene cuando la pérdida de ruta está por debajo del valor de umbral bajo (low_threshold).
 
Utiliza el controlador Bluetooth de ESP32 y la pila BLE host basada en NimBLE.

## Cómo Usar el Ejemplo

Antes de la configuración y compilación del proyecto, asegúrate de establecer el objetivo de chip correcto utilizando:

```bash
idf.py set-target <chip_name>
```

### Hardware Requerido

* Una placa de desarrollo con SoC ESP32/ESP32-C3 (p. ej., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* Un cable USB para alimentación y programación

Consulta [Placas de Desarrollo](https://www.espressif.com/en/products/devkits) para obtener más información al respecto.

### Compilar y Flashear

Ejecuta `idf.py -p PUERTO flash monitor` para compilar, flashear y monitorear el proyecto.

(Para salir del monitor serial, escribe ``Ctrl-]``.)

Consulta la [Guía de Inicio](https://idf.espressif.com/) para conocer los pasos completos para configurar y utilizar ESP-IDF para compilar proyectos.

## Salida del Ejemplo

Esta salida de consola se puede observar cuando proximity_sensor_prph está conectado al cliente:

```
I (356) BLE_INIT: BT controller compile version [a186b41]
I (356) phy_init: phy_version 970,1856f88,May 10 2023,17:44:12
I (416) BLE_INIT: Bluetooth MAC: 84:f7:03:05:a5:f6

I (416) NimBLE_PROX_PRPH: BLE Host Task Started
I (416) NimBLE: GAP procedure initiated: stop advertising.

I (426) NimBLE: Device Address: 
I (426) NimBLE: 84:f7:03:05:a5:f6
I (436) NimBLE: 

I (436) NimBLE: GAP procedure initiated: advertise; 
I (446) NimBLE: disc_mode=2
I (446) NimBLE:  adv_channel_map=0 own_addr_type=0 adv_filter_policy=0 adv_itvl_min=0 adv_itvl_max=0
I (456) NimBLE: 

I (456) main_task: Returned from app_main()
I (4546) NimBLE: connection established; status=0

I (3576) NimBLE: Path loss = -112
I (8576) NimBLE: Path loss = -112
I (13626) NimBLE: Path loss = -120
I (18626) NimBLE: Path loss = -53
I (18626) NimBLE: Path loss exceeded threshold, starting alert for device with conn_handle 1
I (19406) NimBLE: Path loss increased for device connected with conn_handle 1
I (20406) NimBLE: Path loss increased for device connected with conn_handle 1
I (21406) NimBLE: Path loss increased for device connected with conn_handle 1
I (22406) NimBLE: Path loss increased for device connected with conn_handle 1
I (23406) NimBLE: Path loss increased for device connected with conn_handle 1
I (23676) NimBLE: Path loss = -90
I (24406) NimBLE: Path loss increased for device connected with conn_handle 1
I (25406) NimBLE: Path loss increased for device connected with conn_handle 1
I (26406) NimBLE: Path loss increased for device connected with conn_handle 1
I (27406) NimBLE: Path loss increased for device connected with conn_handle 1
I (28406) NimBLE: Path loss increased for device connected with conn_handle 1
I (28676) NimBLE: Path loss = -119
I (28676) NimBLE: Path loss lower than threshold, stopping alert for device with conn_handle 1
I (33676) NimBLE: Path loss = -119
I (38726) NimBLE: Path loss = -119
I (43726) NimBLE: Path loss = -119

```

## Solución de Problemas

Para cualquier consulta técnica, por favor abre un [issue](https://github.com/espressif/esp-idf/issues) en GitHub. Te responderemos lo antes posible.

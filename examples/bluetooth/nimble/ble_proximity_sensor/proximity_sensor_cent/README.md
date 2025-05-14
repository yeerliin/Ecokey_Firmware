| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- |

# Ejemplo de Sensor de Proximidad BLE Central

(Consulta el archivo README.md en el directorio de 'examples' de nivel superior para obtener más información sobre los ejemplos).

Este ejemplo crea un cliente GATT y realiza un escaneo pasivo, luego se conecta al dispositivo periférico si el dispositivo anuncia conectividad y el dispositivo anuncia soporte para el servicio de Pérdida de Enlace (0x1803) como UUID de servicio primario.

Alerta a la aplicación cuando el enlace se desconecta.

Después de la conexión, habilita el emparejamiento y el cifrado de enlace si la bandera `Enable Link Encryption` está establecida en la configuración del ejemplo.

Realiza las siguientes operaciones GATT contra el par especificado:

* Escribe en la característica de nivel de alerta del servicio de pérdida de enlace.

* Después de completar la operación de escritura, lee la característica de nivel de potencia de transmisión.

* Calcula continuamente la pérdida de ruta. Si la pérdida de ruta excede el umbral alto, escribe en la característica de nivel de alerta del servicio de alerta inmediata del periférico para comenzar a alertar.

* Si la pérdida de ruta cae por debajo del umbral bajo, escribe en la característica de nivel de alerta del servicio de alerta inmediata del periférico para detener la alerta.

Si el par no admite un servicio, característica o descriptor requerido, ¡entonces el par mintió cuando afirmó admitir el servicio de Pérdida de Enlace! Cuando esto sucede, o si un procedimiento GATT falla, esta función termina inmediatamente la conexión.

Utiliza el controlador Bluetooth de ESP32 y la pila BLE host basada en NimBLE.

Este ejemplo tiene como objetivo comprender el descubrimiento de servicios BLE, la conexión, el cifrado y las operaciones de características.

Para probar esta demostración, utiliza cualquier aplicación de servidor GATT BLE que anuncie soporte para el servicio de Pérdida de Enlace (0x1803) y lo incluya en la base de datos GATT.

## Cómo Usar el Ejemplo

Antes de la configuración y compilación del proyecto, asegúrate de establecer el objetivo de chip correcto utilizando:

```bash
idf.py set-target <chip_name>
```

### Hardware Requerido

* Una placa de desarrollo con SoC ESP32/ESP32-C2/ESP32-C3/ESP32-S3 (p. ej., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* Un cable USB para alimentación y programación

Consulta [Placas de Desarrollo](https://www.espressif.com/en/products/devkits) para obtener más información al respecto.

### Configurar el Proyecto

Abre el menú de configuración del proyecto: 

```bash
idf.py menuconfig
```

En el menú `Example Configuration`:

* Cambia la opción `Peer Address` si es necesario.

### Compilar y Flashear

Ejecuta `idf.py -p PUERTO flash monitor` para compilar, flashear y monitorear el proyecto.

(Para salir del monitor serial, escribe ``Ctrl-]``.)

Consulta la [Guía de Inicio](https://idf.espressif.com/) para conocer los pasos completos para configurar y utilizar ESP-IDF para compilar proyectos.

## Salida del Ejemplo

Esta es la salida de la consola en una conexión exitosa:

```
I (358) BLE_INIT: BT controller compile version [a186b41]
I (358) phy_init: phy_version 970,1856f88,May 10 2023,17:44:12
I (418) BLE_INIT: Bluetooth MAC: 7c:df:a1:66:a5:c6

I (418) NimBLE_PROX_CENT: BLE Host Task Started
I (418) NimBLE: GAP procedure initiated: stop advertising.

I (418) NimBLE: GAP procedure initiated: discovery; 
I (428) NimBLE: own_addr_type=0 filter_policy=0 passive=1 limited=0 filter_duplicates=1 
I (438) NimBLE: duration=forever
I (438) NimBLE: 

I (448) main_task: Returned from app_main()
I (478) NimBLE: GAP procedure initiated: connect; 
I (478) NimBLE: peer_addr_type=0 peer_addr=
I (488) NimBLE: 84:f7:03:05:a5:f6
I (488) NimBLE:  scan_itvl=16 scan_window=16 itvl_min=24 itvl_max=40 latency=0 supervision_timeout=256 min_ce_len=0 max_ce_len=0 own_addr_type=0
I (498) NimBLE: 

I (548) NimBLE: Connection established 
I (548) NimBLE: 

I (548) NimBLE: Connection secured

I (558) NimBLE: encryption change event; status=1288 
I (558) NimBLE: GATT procedure initiated: discover all services

I (658) NimBLE: GATT procedure initiated: discover all characteristics; 
I (658) NimBLE: start_handle=1 end_handle=4

I (858) NimBLE: GATT procedure initiated: discover all characteristics; 
I (858) NimBLE: start_handle=5 end_handle=8

I (1058) NimBLE: GATT procedure initiated: discover all characteristics; 
I (1058) NimBLE: start_handle=9 end_handle=65535

I (1258) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1258) NimBLE: chr_val_handle=3 end_handle=4

I (1358) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1358) NimBLE: chr_val_handle=7 end_handle=8

I (1458) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1458) NimBLE: chr_val_handle=11 end_handle=65535

I (1558) NimBLE: Service discovery complete; status=0 conn_handle=1

I (1558) NimBLE: GATT procedure initiated: write; 
I (1558) NimBLE: att_handle=3 len=1
I (1658) NimBLE: Write alert level char completed; status=0 conn_handle=1

I (3707) NimBLE: Read on tx power level char completed; status=0 conn_handle=1
I (3707) NimBLE:  attr_handle=11 value=
I (3707) NimBLE: 0x80

I (5427) NimBLE: Connection handle : 1
I (5427) NimBLE: Current RSSI = -16
I (5427) NimBLE: path loss = -112 pwr lvl = -128 rssi = -16
I (5427) NimBLE: GATT procedure initiated: write no rsp; 
I (5427) NimBLE: att_handle=7 len=4

I (5437) NimBLE: Write to alert level characteristis done

I (10447) NimBLE: Connection handle : 1
I (10447) NimBLE: Current RSSI = -16
I (10447) NimBLE: path loss = -112 pwr lvl = -128 rssi = -16
I (10447) NimBLE: GATT procedure initiated: write no rsp; 
I (10457) NimBLE: att_handle=7 len=4

I (10457) NimBLE: Write to alert level characteristis done

I (15467) NimBLE: Connection handle : 1
I (15467) NimBLE: Current RSSI = -8
I (15467) NimBLE: path loss = -120 pwr lvl = -128 rssi = -8
I (15467) NimBLE: GATT procedure initiated: write no rsp; 
I (15477) NimBLE: att_handle=7 len=4

I (15477) NimBLE: Write to alert level characteristis done

I (20487) NimBLE: Connection handle : 1
I (20487) NimBLE: Current RSSI = -75
I (20487) NimBLE: path loss = -53 pwr lvl = -128 rssi = -75
I (20487) NimBLE: GATT procedure initiated: write no rsp; 
I (20497) NimBLE: att_handle=7 len=4

I (20497) NimBLE: Write to alert level characteristis done

I (25507) NimBLE: Connection handle : 1
I (25507) NimBLE: Current RSSI = -38
I (25507) NimBLE: path loss = -90 pwr lvl = -128 rssi = -38
I (25507) NimBLE: GATT procedure initiated: write no rsp; 
I (25517) NimBLE: att_handle=7 len=4

I (25517) NimBLE: Write to alert level characteristis done

I (30527) NimBLE: Connection handle : 1
I (30527) NimBLE: Current RSSI = -9
I (30527) NimBLE: path loss = -119 pwr lvl = -128 rssi = -9
I (30527) NimBLE: GATT procedure initiated: write no rsp; 
I (30537) NimBLE: att_handle=7 len=4

I (30537) NimBLE: Write to alert level characteristis done

I (35547) NimBLE: Connection handle : 1
I (35547) NimBLE: Current RSSI = -9
I (35547) NimBLE: path loss = -119 pwr lvl = -128 rssi = -9
I (35547) NimBLE: GATT procedure initiated: write no rsp; 
I (35557) NimBLE: att_handle=7 len=4

I (35557) NimBLE: Write to alert level characteristis done

I (40567) NimBLE: Connection handle : 1
I (40567) NimBLE: Current RSSI = -9
I (40567) NimBLE: path loss = -119 pwr lvl = -128 rssi = -9
I (40567) NimBLE: GATT procedure initiated: write no rsp; 
I (40577) NimBLE: att_handle=7 len=4

I (40577) NimBLE: Write to alert level characteristis done

I (45587) NimBLE: Connection handle : 1
I (45587) NimBLE: Current RSSI = -9
I (45587) NimBLE: path loss = -119 pwr lvl = -128 rssi = -9
I (45587) NimBLE: GATT procedure initiated: write no rsp; 
I (45597) NimBLE: att_handle=7 len=4

I (45597) NimBLE: Write to alert level characteristis done

I (50607) NimBLE: Connection handle : 1
I (50607) NimBLE: Current RSSI = -9
I (50607) NimBLE: path loss = -119 pwr lvl = -128 rssi = -9
I (50607) NimBLE: GATT procedure initiated: write no rsp; 
I (50617) NimBLE: att_handle=7 len=4

I (50617) NimBLE: Write to alert level characteristis done

I (8957) NimBLE: GAP procedure initiated: discovery; 
I (8967) NimBLE: own_addr_type=0 filter_policy=0 passive=1 limited=0 filter_duplicates=1 
I (8977) NimBLE: duration=forever
I (8977) NimBLE: 

I (10407) NimBLE: Link lost for device with conn_handle 0
I (15407) NimBLE: Link lost for device with conn_handle 0

```

## Solución de Problemas

Para cualquier consulta técnica, por favor abre un [issue](https://github.com/espressif/esp-idf/issues) en GitHub. Te responderemos lo antes posible.

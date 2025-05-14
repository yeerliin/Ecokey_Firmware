| Objetivos Soportados | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-S2 | ESP32-S3 |
| -------------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- |

# Ejemplo de Wi-Fi SoftAP y Station

(Consulta el archivo README.md en el directorio 'examples' de nivel superior para obtener más información sobre los ejemplos).

Este ejemplo demuestra cómo utilizar el controlador Wi-Fi de ESP para actuar simultáneamente como un Punto de Acceso y como una Estación utilizando las características de SoftAP y Station.
Con NAPT habilitado en la interfaz softAP y la interfaz station configurada como interfaz predeterminada, este ejemplo puede usarse como router nat Wi-Fi.

## Cómo usar el ejemplo
### Configurar el proyecto

Abre el menú de configuración del proyecto (`idf.py menuconfig`).

En el menú `Example Configuration`:

* Configura el Wi-Fi SoftAP.
    * Establece `WiFi AP SSID`.
    * Establece `WiFi AP Password`.

* Configura el Wi-Fi STA.
    * Establece `WiFi Remote AP SSID`.
    * Establece `WiFi Remote AP Password`.

Opcional: Si es necesario, modifica las otras opciones para adaptarlas a tus necesidades.

### Compilar y Flashear

Compila el proyecto y flashéalo en la placa, luego ejecuta la herramienta de monitor para ver la salida serial:

Ejecuta `idf.py -p PORT flash monitor` para compilar, flashear y monitorizar el proyecto.

(Para salir del monitor serial, escribe ``Ctrl-]``).

## Salida del Ejemplo

Aquí está la salida de consola para este ejemplo:

```
I (680) WiFi SoftAP: ESP_WIFI_MODE_AP
I (690) WiFi SoftAP: wifi_init_softap finished. SSID:myssid password:mypassword channel:1
I (690) WiFi Sta: ESP_WIFI_MODE_STA
I (690) WiFi Sta: wifi_init_sta finished.
I (700) phy_init: phy_version 4670,719f9f6,Feb 18 2021,17:07:07
I (800) wifi:mode : sta (58:bf:25:e0:41:00) + softAP (58:bf:25:e0:41:01)
I (800) wifi:enable tsf
I (810) wifi:Total power save buffer number: 16
I (810) wifi:Init max length of beacon: 752/752
I (810) wifi:Init max length of beacon: 752/752
I (820) WiFi Sta: Station started
I (820) wifi:new:<1,1>, old:<1,1>, ap:<1,1>, sta:<1,1>, prof:1
I (820) wifi:state: init -> auth (b0)
I (830) wifi:state: auth -> assoc (0)
E (840) wifi:Association refused temporarily, comeback time 1536 mSec
I (2380) wifi:state: assoc -> assoc (0)
I (2390) wifi:state: assoc -> run (10)
I (2400) wifi:connected with myssid_c3, aid = 1, channel 1, 40U, bssid = 84:f7:03:60:86:1d
I (2400) wifi:security: WPA2-PSK, phy: bgn, rssi: -14
I (2410) wifi:pm start, type: 1

I (2410) wifi:AP's beacon interval = 102400 us, DTIM period = 2
I (3920) WiFi Sta: Got IP:192.168.5.2
I (3920) esp_netif_handlers: sta ip: 192.168.5.2, mask: 255.255.255.0, gw: 192.168.5.1
I (3920) WiFi Sta: connected to ap SSID:myssid_c3 password:mypassword_c3
```

## Solución de Problemas

Para cualquier consulta técnica, por favor abre un [issue](https://github.com/espressif/esp-idf/issues) en GitHub. Te responderemos pronto.

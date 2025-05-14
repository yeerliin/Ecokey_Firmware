| Objetivos Soportados | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-S2 | ESP32-S3 |
| -------------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- |

# Ejemplo del Gestor de Aprovisionamiento Wi-Fi

(Consulta el archivo README.md en el directorio 'examples' de nivel superior para obtener más información sobre los ejemplos).

El ejemplo `wifi_prov_mgr` demuestra el uso del componente gestor `wifi_provisioning` para construir una aplicación de aprovisionamiento.

Para este ejemplo, BLE se ha elegido como el modo de transporte predeterminado, sobre el cual se realizará la comunicación relacionada con el aprovisionamiento. NimBLE se ha configurado como el host predeterminado, pero también puedes cambiar a Bluedroid usando menuconfig -> Components -> Bluetooth -> Bluetooth Host.

> Nota: Dado que ESP32-S2 no admite BLE, SoftAP será el modo de transporte predeterminado en ese caso. Incluso para ESP32, puedes cambiar al transporte SoftAP desde menuconfig.

En el proceso de aprovisionamiento, el dispositivo se configura como una estación Wi-Fi con credenciales específicas. Una vez configurado, el dispositivo conservará la configuración Wi-Fi hasta que se realice un borrado de flash.

Justo después de completar el aprovisionamiento, BLE se apaga y se desactiva para liberar la memoria utilizada por la pila BLE. Sin embargo, esto es específico de este ejemplo, y el usuario puede optar por mantener intacta la pila BLE en su propia aplicación.

`wifi_prov_mgr` utiliza los siguientes componentes:
* `wifi_provisioning`: proporciona el gestor, estructuras de datos y manejadores de endpoints protocomm para la configuración Wi-Fi
* `protocomm`: para comunicación basada en protocolos y establecimiento de sesión segura
* `protobuf`: biblioteca de buffer de protocolo de Google para serialización de estructuras de datos de protocomm
* `bt`: pila BLE de ESP32 para el transporte de paquetes protobuf

Este ejemplo puede utilizarse, tal como está, para añadir un servicio de aprovisionamiento a cualquier aplicación destinada a IoT.

> Nota: Si utilizas este código de ejemplo en tu propio proyecto, en modo BLE, recuerda habilitar la pila BT y la configuración de control BTDM BLE en tu configuración SDK (por ejemplo, utilizando el archivo `sdkconfig.defaults` de este proyecto).

## Cómo usar el ejemplo

### Hardware Requerido

El ejemplo debería poder ejecutarse en cualquier placa de desarrollo ESP32/ESP32-S2 comúnmente disponible.

### Aplicación Requerida

Las aplicaciones de aprovisionamiento están disponibles para varias plataformas. Ver a continuación.

#### Plataforma: Android

Para Android, una aplicación de aprovisionamiento junto con el código fuente está disponible en GitHub: [esp-idf-provisioning-android](https://github.com/espressif/esp-idf-provisioning-android)

#### Plataforma: iOS

Para iOS, una aplicación de aprovisionamiento junto con el código fuente está disponible en GitHub: [esp-idf-provisioning-ios](https://github.com/espressif/esp-idf-provisioning-ios)

#### Plataforma: Linux / Windows / macOS

Para instalar los paquetes de dependencia necesarios, consulta el [archivo README](../../README.md#running-test-python-script-pytest) de nivel superior.

`esp_prov` soporta transporte BLE y SoftAP para plataformas Linux, MacOS y Windows. Para BLE, sin embargo, si no se cumplen las dependencias, el script recurre al modo consola y requiere otra aplicación a través de la cual pueda tener lugar la comunicación. La consola `esp_prov` te guiará a través del proceso de aprovisionamiento para localizar los servicios y características GATT BLE correctos, los valores a escribir y los valores de lectura de entrada.

### Configurar el proyecto

```
idf.py menuconfig
```
* Establece el transporte BLE/Soft AP en las opciones de "Configuración de Ejemplo". ESP32-S2 solo tendrá la opción SoftAP (la opción SoftAP no se puede usar si IPv4 está deshabilitado en lwIP)

### Compilar y Flashear

Compila el proyecto y flashea en la placa, luego ejecuta la herramienta de monitor para ver la salida serial:

```
idf.py -p PORT flash monitor
```

(Para salir del monitor serial, escribe ``Ctrl-]``.)

Consulta la Guía de Inicio Rápido para obtener los pasos completos para configurar y usar ESP-IDF para construir proyectos.

## Salida del Ejemplo

```
I (445) app: Starting provisioning
I (1035) app: Provisioning started
I (1045) wifi_prov_mgr: Provisioning started with service name : PROV_261FCC
```

Asegúrate de anotar el nombre del dispositivo BLE (que comienza con `PROV_`) que se muestra en el registro del monitor serial (por ejemplo, PROV_261FCC). Esto dependerá del ID de MAC y será único para cada dispositivo.

En una terminal separada, ejecuta el script `esp_prov.py` en el directorio `$IDP_PATH/tools/esp_prov` (asegúrate de reemplazar `myssid` y `mypassword` con las credenciales del AP al que se supone que el dispositivo debe conectarse después del aprovisionamiento). Suponiendo la configuración de ejemplo predeterminada, que utiliza el esquema de seguridad protocomm 1 con autenticación basada en PoP (prueba de posesión):

```
python esp_prov.py --transport ble --service_name PROV_261FCC --sec_ver 1 --pop abcd1234 --ssid myssid --passphrase mypassword
```

Para la versión de seguridad 2, se puede usar el siguiente comando:
```
python esp_prov.py --transport ble --service_name PROV_261FCC --sec_ver 2 --sec2_username wifiprov --sec2_pwd abcd1234 --ssid myssid --passphrase mypassword
```

El comando anterior realizará los pasos de aprovisionamiento, y el registro del monitor debería mostrar algo como esto:

```
I (39725) app: Received Wi-Fi credentials
    SSID     : myssid
    Password : mypassword
.
.
.
I (45335) esp_netif_handlers: sta ip: 192.168.43.243, mask: 255.255.255.0, gw: 192.168.43.1
I (45345) app: Provisioning successful
I (45345) app: Connected with IP Address:192.168.43.243
I (46355) app: Hello World!
I (47355) app: Hello World!
I (48355) app: Hello World!
I (49355) app: Hello World!
.
.
.
I (52315) wifi_prov_mgr: Provisioning stopped
.
.
.
I (52355) app: Hello World!
I (53355) app: Hello World!
I (54355) app: Hello World!
I (55355) app: Hello World!
```

**Nota:** Para generar las credenciales para la versión de seguridad 2 (sal y verificador `SRP6a`) para el lado del dispositivo, se puede usar el siguiente comando de ejemplo. La salida se puede usar directamente en este ejemplo.

La opción de configuración `CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE` debe estar habilitada para el ejemplo y en `main/app_main.c`, la macro `EXAMPLE_PROV_SEC2_USERNAME` debe establecerse en el mismo nombre de usuario utilizado en la generación de sal-verificador.

```log
$ python esp_prov.py --transport softap --sec_ver 2 --sec2_gen_cred --sec2_username wifiprov --sec2_pwd abcd1234
==== Salt-verifier for security scheme 2 (SRP6a) ====
static const char sec2_salt[] = {
    0x03, 0x6e, 0xe0, 0xc7, 0xbc, 0xb9, 0xed, 0xa8, 0x4c, 0x9e, 0xac, 0x97, 0xd9, 0x3d, 0xec, 0xf4
};

static const char sec2_verifier[] = {
    0x7c, 0x7c, 0x85, 0x47, 0x65, 0x08, 0x94, 0x6d, 0xd6, 0x36, 0xaf, 0x37, 0xd7, 0xe8, 0x91, 0x43,
    0x78, 0xcf, 0xfd, 0x61, 0x6c, 0x59, 0xd2, 0xf8, 0x39, 0x08, 0x12, 0x72, 0x38, 0xde, 0x9e, 0x24,
    .
    .
    .
    0xe6, 0xf6, 0x53, 0xc8, 0x31, 0xa8, 0x78, 0xde, 0x50, 0x40, 0xf7, 0x62, 0xde, 0x36, 0xb2, 0xba
};

```

### Escaneo de Código QR

Habilitar `CONFIG_EXAMPLE_PROV_SHOW_QR` mostrará un código QR en la terminal serial, que se puede escanear desde las aplicaciones de teléfono de Aprovisionamiento ESP para iniciar el proceso de aprovisionamiento Wi-Fi.

El registro del monitor debería mostrar algo como esto:

```
I (1462) app: Provisioning started
I (1472) app: Scan this QR code from the provisioning application for Provisioning.
I (1472) QRCODE: Encoding below text with ECC LVL 0 & QR Code Version 10
I (1482) QRCODE: {"ver":"v1","name":"PROV_EA69FC","pop":"abcd1234","transport":"ble"}
GAP procedure initiated: advertise; disc_mode=2 adv_channel_map=0 own_addr_type=0 adv_filter_policy=0 adv_itvl_min=256 adv_itvl_max=256

  █▀▀▀▀▀█ ▀▀▀█▄█   ▀▀▄ █▄ ▀ █▀▀▀▀▀█
  █ ███ █  ▀▄█ █▄ ▀▄█ ▄██ █ █ ███ █
  █ ▀▀▀ █  ▄▀█▀▄▀ ▀█▄▀  ██  █ ▀▀▀ █
  ▀▀▀▀▀▀▀ █▄▀ █▄█▄█ ▀ █ █ ▀ ▀▀▀▀▀▀▀
  ▀▀ ▀▀ ▀  ▀▄ ▀▄ ▄▀▀▀█ ▀▄ ▀ ▀▄▄ ▄▄▀
  ███▄█▄▀ █▀  ▀▀▀▀▄▄█   █▀ █  ▄█▄█▀
  ▀███▀ ▀▄▄██ ▄▄██▄ ▀▀▀▀   ▄▀█ ▀▄▄▀
  ▄███  ▀██▀▀ ▄ ▄█▄▀▀█▄ ▀▄▀▄▄█  ▄
  ▀█▀ █▄▀▀ ▀▀█▀▀ █▀▄▀▄▀ ▄█  ███▄ ██
  ██▀█  ▀▄█ █▄▀▄███▀▄▀█ ▀█ █▀▀ ▀▄▄▀
  █▄▀▄█▀▀ ▀▄ ▀▄▄█▄▀▀█▄█▄█▀▀█ ▀▄ ▄▀
  █ ▄█▄ ▀ ▄▀ █▄  ▀█▄█▄▀▀█▀█ ▄█ ▀▄▄█
  ▀▀▀▀  ▀ █▀█▀▀▄▄██▄█▀█ ▀██▀▀▀█▄▄▀
  █▀▀▀▀▀█   ▄█▀▀▀██ ▄▀▄ █▄█ ▀ █ ▄ ▄
  █ ███ █ █ ▀▄█▀▀█▀▄█▄▄ ▀██▀▀▀▀▄▄▀▀
  █ ▀▀▀ █ ▄█ ▀ ▄█▀█ █▀ ▀▀███▄▀█ █▄█
  ▀▀▀▀▀▀▀ ▀  ▀  ▀▀ ▀     ▀▀▀▀▀▀


I (1702) app: If QR code is not visible, copy paste the below URL in a browser.
https://espressif.github.io/esp-jumpstart/qrcode.html?data={"ver":"v1","name":"PROV_EA69FC","pop":"abcd1234","transport":"ble"}
```


### Escaneo Wi-Fi

El gestor de aprovisionamiento también admite proporcionar resultados de escaneo Wi-Fi en tiempo real (realizados en el dispositivo) durante el aprovisionamiento. Esto permite que las aplicaciones del lado del cliente elijan el AP para el cual se configurará la estación Wi-Fi del dispositivo. Se dispone de diversa información sobre los AP visibles, como la intensidad de la señal (RSSI) y el tipo de seguridad, etc. Además, el gestor ahora proporciona información de capacidades que puede ser utilizada por las aplicaciones cliente para determinar el tipo de seguridad y la disponibilidad de características específicas (como `wifi_scan`).

Al usar el aprovisionamiento basado en escaneo, no necesitamos especificar explícitamente los campos `--ssid` y `--passphrase`:

```
python esp_prov.py --transport ble --service_name PROV_261FCC --pop abcd1234
```

Consulta a continuación la salida de muestra de la herramienta `esp_prov` al ejecutar el comando anterior:

```
Connecting...
Connected
Getting Services...
Security scheme determined to be : 1

==== Starting Session ====
==== Session Established ====

==== Scanning Wi-Fi APs ====
++++ Scan process executed in 1.9967520237 sec
++++ Scan results : 5

++++ Scan finished in 2.7374596596 sec
==== Wi-Fi Scan results ====
S.N. SSID                              BSSID         CHN RSSI AUTH
[ 1] MyHomeWiFiAP                      788a20841996    1 -45  WPA2_PSK
[ 2] MobileHotspot                     7a8a20841996   11 -46  WPA2_PSK
[ 3] MyHomeWiFiAP                      788a208daa26   11 -54  WPA2_PSK
[ 4] NeighborsWiFiAP                   8a8a20841996    6 -61  WPA2_PSK
[ 5] InsecureWiFiAP                    dca4caf1227c    7 -74  Open

Select AP by number (0 to rescan) : 1
Enter passphrase for MyHomeWiFiAP :

==== Sending Wi-Fi Credentials to Target ====
==== Wi-Fi Credentials sent successfully ====

==== Applying Wi-Fi Config to Target ====
==== Apply config sent successfully ====

==== Wi-Fi connection state  ====
==== WiFi state: Connected ====
==== Provisioning was successful ====
```

### Aprovisionamiento Interactivo

`esp_prov` admite aprovisionamiento interactivo. Puedes activar el script con un comando simplificado e ingresar los detalles necesarios
(`Prueba de posesión` para el esquema de seguridad 1 y `nombre de usuario SRP6a`, `contraseña SRP6a` para el esquema de seguridad 2) a medida que avanza el proceso de aprovisionamiento.

El comando `python esp_prov.py --transport ble --sec_ver 1` da la siguiente salida de muestra:

```
Discovering...
==== BLE Discovery results ====
S.N. Name                              Address
[ 1] PROV_4C33E8                       01:02:03:04:05:06
[ 1] BT_DEVICE_SBC                     0A:0B:0C:0D:0E:0F
Select device by number (0 to rescan) : 1
Connecting...
Getting Services...
Proof of Possession required:

==== Starting Session ====
==== Session Established ====

==== Scanning Wi-Fi APs ====
++++ Scan process executed in 3.8695244789123535 sec
++++ Scan results : 2

++++ Scan finished in 4.4132080078125 sec
==== Wi-Fi Scan results ====
S.N. SSID                              BSSID         CHN RSSI AUTH
[ 1] MyHomeWiFiAP                      788a20841996    1 -45  WPA2_PSK
[ 2] MobileHotspot                     7a8a20841996   11 -46  WPA2_PSK

Select AP by number (0 to rescan) : 1
Enter passphrase for myssid :

==== Sending Wi-Fi Credentials to Target ====
==== Wi-Fi Credentials sent successfully ====

==== Applying Wi-Fi Config to Target ====
==== Apply config sent successfully ====

==== Wi-Fi connection state  ====
==== WiFi state: Connected ====
==== Provisioning was successful ====
```

### Envío de Datos Personalizados

El gestor de aprovisionamiento permite a las aplicaciones enviar algunos datos personalizados durante el aprovisionamiento, que pueden ser
necesarios para algunas otras operaciones como conectarse a algún servicio en la nube. Esto se logra creando
y registrando endpoints adicionales utilizando las siguientes API

```
wifi_prov_mgr_endpoint_create();
wifi_prov_mgr_endpoint_register();
```

En este ejemplo en particular, hemos añadido un endpoint llamado "custom-data" que se puede probar
pasando la opción `--custom_data <MyCustomData>` a la herramienta esp\_prov. Se espera la siguiente salida en caso de éxito:

```
==== Sending Custom data to esp32 ====
CustomData response: SUCCESS
```

## Solución de Problemas

### Aprovisionamiento fallido

Es posible que las credenciales Wi-Fi proporcionadas sean incorrectas, o que el dispositivo no haya podido establecer conexión con la red, en cuyo caso el script `esp_prov` notificará el fallo (con motivo). El registro del monitor serial mostrará el fallo junto con el motivo de desconexión:

```
E (367015) app: Provisioning failed!
    Reason : Wi-Fi AP password incorrect
    Please reset to factory and retry provisioning
```

Una vez que se hayan aplicado las credenciales, aunque se hayan proporcionado credenciales incorrectas, el dispositivo ya no entrará en modo de aprovisionamiento en reinicios posteriores hasta que se borre el NVS (consulta la siguiente sección).

### El aprovisionamiento no se inicia

Si el registro del monitor serial muestra lo siguiente:

```
I (465) app: Already provisioned, starting Wi-Fi STA
```

significa que el dispositivo ya ha sido aprovisionado anteriormente con o sin éxito (por ejemplo, el escenario cubierto en la sección anterior), o que las credenciales Wi-Fi ya fueron configuradas por alguna otra aplicación que se flasheó previamente en tu dispositivo. Al establecer el nivel de registro en DEBUG, esto es claramente evidente:

```
D (455) wifi_prov_mgr: Found Wi-Fi SSID     : myssid
D (465) wifi_prov_mgr: Found Wi-Fi Password : m********d
I (465) app: Already provisioned, starting Wi-Fi STA
```

Para solucionar esto, simplemente necesitamos borrar la partición NVS del flash. Primero necesitamos encontrar su dirección y tamaño. Esto se puede ver en el registro del monitor en la parte superior derecha después del reinicio.

```
I (47) boot: Partition Table:
I (50) boot: ## Label            Usage          Type ST Offset   Length
I (58) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (65) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (73) boot:  2 factory          factory app      00 00 00010000 00124f80
I (80) boot: End of partition table
```

Ahora borra la partición NVS ejecutando los siguientes comandos:

```
$IDF_PATH/components/esptool_py/esptool/esptool.py erase_region 0x9000 0x6000
```

### Solicitud de Emparejamiento Bluetooth durante el aprovisionamiento

ESP-IDF ahora tiene la funcionalidad de imponer el requisito de cifrado de enlace al realizar una escritura GATT en las características del servicio de aprovisionamiento. Sin embargo, esto resultará en un cuadro de diálogo de emparejamiento, si el enlace no está cifrado. Esta función está deshabilitada de forma predeterminada. Para habilitar esta función, establece `CONFIG_WIFI_PROV_BLE_FORCE_ENCRYPTION=y` en el sdkconfig o selecciona la configuración usando "idf.py menuconfig".

```
Component Config --> Wi-Fi Provisioning Manager --> Force Link Encryption during Characteristic Read/Write

```
Recompilar la aplicación con los cambios anteriores debería ser suficiente para habilitar esta funcionalidad.


### Plataforma no soportada

Si no se cumple el requisito de plataforma para ejecutar `esp_prov`, la ejecución del script recurrirá al modo consola, en cuyo caso el proceso completo (que implica entradas del usuario) se verá así:

```
==== Esp_Prov Version: v1.0 ====
BLE client is running in console mode
    This could be due to your platform not being supported or dependencies not being met
    Please ensure all pre-requisites are met to run the full fledged client
BLECLI >> Please connect to BLE device `PROV_261FCC` manually using your tool of choice
BLECLI >> Was the device connected successfully? [y/n] y
BLECLI >> List available attributes of the connected device
BLECLI >> Is the service UUID '0000ffff-0000-1000-8000-00805f9b34fb' listed among available attributes? [y/n] y
BLECLI >> Is the characteristic UUID '0000ff53-0000-1000-8000-00805f9b34fb' listed among available attributes? [y/n] y
BLECLI >> Is the characteristic UUID '0000ff51-0000-1000-8000-00805f9b34fb' listed among available attributes? [y/n] y
BLECLI >> Is the characteristic UUID '0000ff52-0000-1000-8000-00805f9b34fb' listed among available attributes? [y/n] y

==== Verifying protocol version ====
BLECLI >> Write following data to characteristic with UUID '0000ff53-0000-1000-8000-00805f9b34fb' :
    >> 56302e31
BLECLI >> Enter data read from characteristic (in hex) :
    << 53554343455353
==== Verified protocol version successfully ====

==== Starting Session ====
BLECLI >> Write following data to characteristic with UUID '0000ff51-0000-1000-8000-00805f9b34fb' :
    >> 10015a25a201220a20ae6d9d5d1029f8c366892252d2d5a0ffa7ce1ee5829312545dd5f2aba057294d
BLECLI >> Enter data read from characteristic (in hex) :
    << 10015a390801aa0134122048008bfc365fad4753dc75912e0c764d60749cb26dd609595b6fbc72e12614031a1089733af233c7448e7d7fb7963682c6d8
BLECLI >> Write following data to characteristic with UUID '0000ff51-0000-1000-8000-00805f9b34fb' :
    >> 10015a270802b2012212204051088dc294fe4621fac934a8ea22e948fcc3e8ac458aac088ce705c65dbfb9
BLECLI >> Enter data read from characteristic (in hex) :
    << 10015a270803ba01221a20c8d38059d5206a3d92642973ac6ba8ac2f6ecf2b7a3632964eb35a0f20133adb
==== Session Established ====

==== Sending Wifi credential to esp32 ====
BLECLI >> Write following data to characteristic with UUID '0000ff52-0000-1000-8000-00805f9b34fb' :
    >> 98471ac4019a46765c28d87df8c8ae71c1ae6cfe0bc9c615bc6d2c
BLECLI >> Enter data read from characteristic (in hex) :
    << 3271f39a
==== Wifi Credentials sent successfully ====

==== Applying config to esp32 ====
BLECLI >> Write following data to characteristic with UUID '0000ff52-0000-1000-8000-00805f9b34fb' :
    >> 5355
BLECLI >> Enter data read from characteristic (in hex) :
    << 1664db24
==== Apply config sent successfully ====

==== Wifi connection state  ====
BLECLI >> Write following data to characteristic with UUID '0000ff52-0000-1000-8000-00805f9b34fb' :
    >> 290d
BLECLI >> Enter data read from characteristic (in hex) :
    << 505f72a9f8521025c1964d7789c4d7edc56aedebd144e1b667bc7c0975757b80cc091aa9f3e95b06eaefbc30290fa1
++++ WiFi state: connected ++++
==== Provisioning was successful ====
```

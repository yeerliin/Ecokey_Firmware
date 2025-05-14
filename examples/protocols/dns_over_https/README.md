| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- |

# Ejemplo de DNS sobre HTTPS

Este ejemplo demuestra cómo utilizar el componente DNS sobre HTTPS (DoH) en una aplicación ESP32. El ejemplo resuelve nombres de dominio de manera segura a través de HTTPS utilizando el proveedor de DNS sobre HTTPS configurado (Google (por defecto), Cloudflare o un servidor personalizado).

## Características

- **DNS sobre HTTPS**: Resuelve nombres de dominio de forma segura utilizando HTTPS.

## Opciones de Certificado
Este ejemplo proporciona dos opciones de certificado para DNS sobre HTTPS:

1. Paquete de Certificados Interno (Predeterminado): Por defecto, el ejemplo utiliza un paquete de certificados interno, facilitando el uso de proveedores populares de DoH como Google y Cloudflare.
2. Certificado Personalizado: Si prefieres utilizar tu propio servidor DoH y certificado, puedes configurar el nombre del servidor en menuconfig y colocar tu certificado personalizado en el archivo `cert_custom_root.pem`. Esta opción proporciona flexibilidad si tienes requisitos específicos de seguridad o servidor.

Para configurar la opción de certificado en menuconfig, navega a `Example DNS-over-HTTPS Configuration → Use internal certificate bundle` para habilitar o deshabilitar el paquete interno.

## Configuración

Antes de compilar y ejecutar el ejemplo, es necesario configurar el proveedor de DNS sobre HTTPS en `menuconfig`:

1. Ejecuta `idf.py menuconfig`.
2. Busca la sección **Example DNS-over-HTTPS Configuration**.
3. Elige tu proveedor de DNS preferido:
    * Google DNS (predeterminado: dns.google)
    * Cloudflare DNS (cloudflare-dns.com)
    * Servidor DNS-over-HTTPS personalizado
4. Para la configuración de DNS personalizado, introduce la URL del servidor DNS-over-HTTPS personalizado.
5. Especifica una ruta de consulta DNS-over-HTTPS personalizada (predeterminado: dns-query).
6. Activa o desactiva el uso del paquete de certificados interno:
    * Si está desactivado, especifica el certificado DNS (cert_custom_root.pem).
7. Guarda los cambios y sal del menú de configuración.
8. Configura Wi-Fi o Ethernet para unirte a una red. Consulta la sección "Establecimiento de Conexión Wi-Fi o Ethernet" en [examples/protocols/README.md](../../README.md) para más detalles.

## Cómo Funciona

1. **Inicialización de Red**: La aplicación inicializa las interfaces de red (Wi-Fi o Ethernet) y establece una conexión.
2. **Inicialización de NVS**: Se inicializa el Almacenamiento No Volátil (NVS) para almacenar y recuperar la hora del sistema entre reinicios.
3. **Inicialización de DNS sobre HTTPS**: La función `init_dns_over_https()` inicializa el resolvedor DNS sobre HTTPS, que maneja de forma segura las consultas DNS utilizando HTTPS.
4. **Realizando getaddrinfo**: La aplicación ejecuta la operación getaddrinfo para varios nombres de dominio.

## Cómo usar el ejemplo
Antes de la configuración y compilación del proyecto, asegúrate de establecer el objetivo de chip correcto usando `idf.py set-target <chip_name>`.

### Hardware Requerido

* Una placa de desarrollo con SoC ESP32/ESP32-S2/ESP32-C3 (p. ej., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* Un cable USB para alimentación y programación

### Compilar y Flashear

Compila el proyecto y flashéalo en la placa, luego ejecuta la herramienta de monitor para ver la salida serial:

```
idf.py -p PUERTO flash monitor
```

(Reemplaza PUERTO con el nombre del puerto serial a utilizar.)

(Para salir del monitor serial, escribe ``Ctrl-]``.)

Consulta la Guía de Inicio para conocer los pasos completos para configurar y utilizar ESP-IDF para compilar proyectos.


## Consejos de Solución de Problemas

* **Conectividad**:
	Asegúrate de que los detalles de conexión de red sean precisos. Por ejemplo, verifica el SSID y la contraseña de Wi-Fi o comprueba que la conexión Ethernet esté segura y no sea defectuosa.

* **Errores de Desbordamiento de Pila**:
	Si encuentras un desbordamiento de pila, podría indicar que el servidor DNS está devolviendo un mensaje de error extenso. Para diagnosticar el problema, intenta aumentar el tamaño de la pila para obtener información más detallada.
 * **Problemas de Ruta Incorrecta**:
	Ocasionalmente, el error de desbordamiento de pila es causado por una ruta de servidor incorrecta. Verifica los detalles del servidor y asegúrate de que la ruta del servidor esté configurada correctamente.


## Ejemplo de Salida

```
I (4652) esp_netif_handlers: example_netif_sta ip: 192.168.50.136, mask: 255.255.255.0, gw: 192.168.50.1
I (4652) example_connect: Got IPv4 event: Interface "example_netif_sta" address: 192.168.50.136
I (5552) example_connect: Got IPv6 event: Interface "example_netif_sta" address: fe80:0000:0000:0000:5abf:25ff:fee0:4100, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (5552) example_common: Connected to example_netif_sta
I (5562) example_common: - IPv4 address: 192.168.50.136,
I (5562) example_common: - IPv6 address: fe80:0000:0000:0000:5abf:25ff:fee0:4100, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (5572) time_sync: Updating time from NVS
I (5582) main_task: Returned from app_main()
I (5592) wifi:<ba-add>idx:1 (ifx:0, a0:36:bc:0e:c4:f0), tid:7, ssn:3, winSize:64
I (5622) wifi:<ba-del>idx:0, tid:6
I (5622) wifi:<ba-add>idx:0 (ifx:0, a0:36:bc:0e:c4:f0), tid:0, ssn:1, winSize:64
I (5962) esp-x509-crt-bundle: Certificate validated
I (6772) example_dns_over_https: Using DNS Over HTTPS server: dns.google
I (6772) example_dns_over_https: Resolving IP addresses for yahoo.com:
I (6772) example_dns_over_https: IPv4: 74.6.143.26
I (6782) example_dns_over_https: IPv4: 74.6.231.21
I (6782) example_dns_over_https: IPv4: 98.137.11.163
I (6792) example_dns_over_https: IPv4: 74.6.231.20

I (7192) esp-x509-crt-bundle: Certificate validated
I (8382) example_dns_over_https: Using DNS Over HTTPS server: dns.google
I (8382) example_dns_over_https: Resolving IP addresses for www.google.com:
I (8382) example_dns_over_https: IPv6: 2404:6800:4015:803::2004

I (9032) esp-x509-crt-bundle: Certificate validated
I (9862) example_dns_over_https: Using DNS Over HTTPS server: dns.google
I (9862) example_dns_over_https: Resolving IP addresses for www.google.com:
I (9862) example_dns_over_https: IPv4: 142.250.70.228
```

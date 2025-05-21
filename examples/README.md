# Ejemplos

Este directorio contiene una variedad de proyectos de ejemplo de ESP-IDF. Estos ejemplos están destinados a demostrar partes de la funcionalidad de ESP-IDF y proporcionar código que puedes copiar y adaptar a tus propios proyectos.

## Estructura de los Ejemplos

Los ejemplos están agrupados en subdirectorios por categoría. Cada directorio de categoría contiene uno o más proyectos de ejemplo:

- `bluetooth/bluedroid` Ejemplos de BT Clásico, BLE y coexistencia usando la pila de host predeterminada Bluedroid.
- `bluetooth/nimble` Ejemplos de BLE usando la pila de host NimBLE.
- `bluetooth/esp_ble_mesh` Ejemplos de ESP BLE Mesh.
- `bluetooth/hci` Ejemplos de transporte HCI (VHCI y HCI UART).
- `build_system` Ejemplos de características del sistema de compilación.
- `cxx` Ejemplos de utilización del lenguaje C++ y componentes experimentales.
- `ethernet` Ejemplos de red Ethernet.
- `get-started` Ejemplos sencillos con funcionalidad mínima. Buen punto de partida para principiantes.
- `ieee802154` Ejemplos de IEEE802.15.4.
- `mesh` Ejemplos de malla Wi-Fi.
- `network` Ejemplos relacionados con el entorno de red general, pruebas y análisis.
- `openthread` Ejemplos de OpenThread.
- `peripherals` Ejemplos que muestran la funcionalidad del controlador para los diversos periféricos integrados del ESP32.
- `protocols` Ejemplos que muestran interacciones de protocolos de red.
- `provisioning` Ejemplos de aprovisionamiento Wi-Fi.
- `security` Ejemplos sobre características de seguridad.
- `storage` Ejemplos que muestran métodos de almacenamiento de datos usando memoria flash SPI, almacenamiento externo como la interfaz SD/MMC y particionamiento flash.
- `system` Demuestra algunas características internas del chip, o herramientas de depuración y desarrollo.
- `wifi` Características avanzadas de Wi-Fi (Para ejemplos de protocolos de red, consulte `protocols`).
- `Zigbee` Ejemplos de red y dispositivos Zigbee.

Además de estos ejemplos, el directorio `commmon_components` contiene código compartido por varios ejemplos.

## Uso de los Ejemplos

Antes de construir un ejemplo, sigue la [Guía de inicio de ESP-IDF](https://idf.espressif.com/) para asegurarte de tener el entorno de desarrollo requerido.

### Configurar el Chip Objetivo

En primer lugar, tu objetivo debe ser compatible con:

- **Tu versión de ESP-IDF**: Para la lista completa de objetivos compatibles, ejecuta:
  ```
  idf.py --list-targets
  ```
- **Este ejemplo**: Para la lista completa de objetivos compatibles, consulta la tabla de objetivos compatibles en la parte superior de este README.

Después de asegurarte de que tu objetivo es compatible, ve al directorio de tu proyecto de ejemplo y [configura el chip objetivo](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/tools/idf-py.html#select-the-target-chip-set-target):

```
idf.py set-target <target>
```

Por ejemplo, para establecer esp32 como el chip objetivo, ejecuta:

```
idf.py set-target esp32
```


### Configurar el Proyecto

Para información sobre las opciones de Kconfig, consulta [Configuración del Proyecto](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/kconfig.html).

Para verificar o modificar cómodamente las opciones de Kconfig para este ejemplo en un menú de configuración del proyecto, ejecuta:

```
idf.py menuconfig
```


### Compilar y Flashear

Ejecuta el siguiente comando para compilar el proyecto, flashearlo en tu placa de desarrollo y ejecutar la herramienta de monitoreo para ver la salida serial:

```
idf.py build flash monitor
```

Este comando puede reducirse a `idf.py flash monitor`.

Si el comando anterior falla, revisa el registro en el monitor serial que generalmente proporciona información sobre la posible causa del problema.

Para salir del monitor serial, usa `Ctrl` + `]`.


## Ejecutar Script de Prueba Python (pytest)

Algunos de los ejemplos tienen scripts `pytest_....py` que utilizan `pytest` como framework de pruebas. Para información detallada, consulta la sección "Ejecutar las Pruebas Localmente" en la [documentación de pruebas ESP-IDF con Pytest](../docs/en/contribute/esp-idf-tests-with-pytest.rst)

Usar `pytest` es la forma recomendada de escribir nuevas pruebas. Migraremos todos los scripts de prueba de ejemplo a este nuevo framework pronto.

## Copiando Ejemplos

Cada ejemplo es un proyecto independiente. Los ejemplos _no tienen que estar dentro del directorio esp-idf_. Puedes copiar un directorio de ejemplo a cualquier lugar en tu computadora para hacer una copia que puedas modificar y con la que puedas trabajar.

La variable de entorno `IDF_PATH` es lo único que conecta el ejemplo con el resto de ESP-IDF.

Si estás buscando un proyecto más básico para comenzar, prueba [esp-idf-template](https://github.com/espressif/esp-idf-template).

## Desarrollo Adicional

Para pasos adicionales sobre cómo desarrollar un proyecto, consulta lo siguiente:

- Gestión del proyecto:
  - Documento [IDF Frontend](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project)
  - Video de introducción a ESP-IDF ([YouTube](https://youtu.be/J8zc8mMNKtc?t=340), [bilibili](https://www.bilibili.com/video/BV1114y1r7du/?t=336))
  - [Descripción general](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#example-project) de un proyecto de ejemplo
  - Documento del [Sistema de Compilación](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html)
- Escribir código:
  - Escribe tu propio código siguiendo las [referencias de API](https://docs.espressif.com/projects/esp-idf/en/stable/api-reference/index.html)

## Contribuyendo con Ejemplos

Si tienes un nuevo ejemplo que crees que nos gustaría, considera enviárnoslo como un Pull Request.

En la documentación de ESP-IDF, puedes encontrar una página ["Creando Ejemplos"](../docs/en/contribute/creating-examples.rst) que establece los pasos para crear un ejemplo de alta calidad.

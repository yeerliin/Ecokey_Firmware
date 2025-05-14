# Ejemplos

Este directorio contiene una serie de proyectos de ejemplo ESP-IDF. Estos ejemplos están destinados a demostrar partes de la funcionalidad de ESP-IDF, y proporcionar código que puedes copiar y adaptar a tus propios proyectos.

## Estructura de Ejemplos

Los ejemplos están agrupados en subdirectorios por categoría. Cada directorio de categoría contiene uno o más proyectos de ejemplo:

- `bluetooth/bluedroid` Ejemplos de BT Clásico, BLE y coexistencia utilizando la pila de host predeterminada Bluedroid.
- `bluetooth/nimble` Ejemplos de BLE utilizando la pila de host NimBLE.
- `bluetooth/esp_ble_mesh` Ejemplos de ESP BLE Mesh.
- `bluetooth/hci` Ejemplos de transporte HCI (VHCI y HCI UART).
- `build_system` Ejemplos de características del sistema de compilación.
- `cxx` Ejemplos de utilización del lenguaje C++ y componentes experimentales.
- `ethernet` Ejemplos de red Ethernet.
- `get-started` Ejemplos simples con funcionalidad mínima. Buen punto de partida para principiantes.
- `ieee802154` Ejemplos de IEEE802.15.4.
- `mesh` Ejemplos de Wi-Fi Mesh.
- `network` Ejemplos relacionados con el entorno general de red, pruebas y análisis.
- `openthread` Ejemplos de OpenThread.
- `peripherals` Ejemplos que muestran la funcionalidad del controlador para los diversos periféricos a bordo del ESP32.
- `protocols` Ejemplos que muestran interacciones de protocolos de red.
- `provisioning` Ejemplos de aprovisionamiento Wi-Fi.
- `security` Ejemplos sobre características de seguridad.
- `storage` Ejemplos que muestran métodos de almacenamiento de datos utilizando flash SPI, almacenamiento externo como la interfaz SD/MMC y particionamiento de flash.
- `system` Demuestra algunas características internas del chip, o herramientas de depuración y desarrollo.
- `wifi` Características avanzadas de Wi-Fi (Para ejemplos de protocolos de red, consulta `protocols`).
- `Zigbee` Ejemplos de red y dispositivo Zigbee.

Además de estos ejemplos, el directorio `commmon_components` contiene código compartido por varios ejemplos.

## Uso de los Ejemplos

Antes de compilar un ejemplo, sigue la [Guía de Inicio de ESP-IDF](https://idf.espressif.com/) para asegurarte de tener el entorno de desarrollo requerido.

### Establecer el Objetivo del Chip

En primer lugar, tu objetivo debe ser compatible con:

- **Tu versión de ESP-IDF**: Para ver la lista completa de objetivos compatibles, ejecuta:
  ```
  idf.py --list-targets
  ```
- **Este ejemplo**: Para ver la lista completa de objetivos compatibles, consulta la tabla de objetivos compatibles en la parte superior de este README.

Después de asegurarte de que tu objetivo es compatible, ve al directorio de tu proyecto de ejemplo y [establece el objetivo del chip](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/tools/idf-py.html#select-the-target-chip-set-target):

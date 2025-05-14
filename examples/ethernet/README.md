| Objetivos Soportados | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| -------------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# Ejemplos de Ethernet

Consulta el archivo [README.md](../README.md) en el directorio [examples](../) de nivel superior para obtener más información sobre los ejemplos.

## Asignaciones de Pines Comunes

### Usando el MAC interno de ESP32

* El cableado RMII es fijo y no se puede cambiar a través de IOMUX o la Matriz GPIO. Por defecto, están conectados de la siguiente manera:

| GPIO   | Señal RMII  | Notas        |
| ------ | ----------- | ------------ |
| GPIO21 | TX_EN       | EMAC_TX_EN   |
| GPIO19 | TX0         | EMAC_TXD0    |
| GPIO22 | TX1         | EMAC_TXD1    |
| GPIO25 | RX0         | EMAC_RXD0    |
| GPIO26 | RX1         | EMAC_RXD1    |
| GPIO27 | CRS_DV      | EMAC_RX_DRV  |

* Uno de los siguientes pines GPIO se puede usar como entrada/salida de RMII REF_CLK:

| GPIO   | Función             | Notas         |
| ------ | ------------------- | ------------- |
| GPIO0  | EMAC_TX_CLK/CLK_OUT1 | entrada/salida |
| GPIO16 | EMAC_CLK_OUT         | salida        |
| GPIO17 | EMAC_CLK_180         | salida        |

* El cableado SMI (Serial Management Interface) no es fijo. Es posible que debas cambiarlo de acuerdo con el esquema de tu placa. Por defecto, están conectados de la siguiente manera:

| GPIO   | Señal SMI  | Notas           |
| ------ | ---------- | --------------- |
| GPIO23 | MDC        | Salida al PHY   |
| GPIO18 | MDIO       | Bidireccional   |

* El chip PHY tiene un pin de reset, si quieres hacer un reset por hardware durante la inicialización, tendrás que conectarlo con un GPIO en el ESP32. Ver más información [aquí](#configure-the-project). El GPIO predeterminado utilizado para resetear el chip PHY es GPIO5.

### Usando módulos ethernet SPI

* Los módulos Ethernet SPI (DM9051, W5500, ...) típicamente consumen una interfaz SPI más un GPIO de interrupción y reset. Pueden conectarse de la siguiente manera para ESP32 como ejemplo. Sin embargo, pueden remapearse a cualquier pin usando la Matriz GPIO.

| GPIO   | DM9051      |
| ------ | ----------- |
| GPIO14 | SPI_CLK     |
| GPIO13 | SPI_MOSI    |
| GPIO12 | SPI_MISO    |
| GPIO15 | SPI_CS      |
| GPIO4  | Interrupción|
| NC     | Reset       |

---

**Advertencia:**
Consulta el manual de referencia técnica de Espressif junto con la hoja de datos del módulo ESP específico que uses al asignar cualquier otro pin, especialmente al elegir desde el menú de configuración del sistema para los ejemplos de ethernet, algunos pines no se pueden usar (pueden estar ya utilizados para un propósito diferente como Flash/RAM SPI, algunos pines podrían ser solo entradas, etc.).

---

## Configuraciones Comunes

1. En el menú `Example Ethernet Configuration`:
    * Elige el tipo de Ethernet.
    * Si se selecciona `Internal EMAC`:
        * Elige el dispositivo PHY en `Ethernet PHY Device`, por defecto, el **ESP32-Ethernet-Kit** tiene un `IP101` a bordo.
        * Establece el número de GPIO utilizado por la señal SMI en `SMI MDC GPIO number` y `SMI MDIO GPIO number` respectivamente.
    * Si se selecciona `SPI Ethernet`:
        * Establece la configuración específica del SPI, incluidos el número de host SPI, los números de GPIO y la velocidad de reloj.
        * Se pueden conectar múltiples módulos Ethernet SPI del mismo tipo a una única interfaz SPI a la vez. Los módulos comparten las señales de datos y CLK. Los pines CS, interrupción y reset deben configurarse específicamente para cada módulo por separado.
    * Establece el número de GPIO utilizado por el reset del chip PHY en `PHY Reset GPIO number`, es posible que tengas que cambiar el valor predeterminado según el esquema de tu placa. **El reset por hardware del PHY puede desactivarse estableciendo este valor en -1**.
    * Establece la dirección PHY en `PHY Address`, es posible que tengas que cambiar el valor predeterminado según el esquema de tu placa.

2. En el menú `Component config > Ethernet`:
    * En el submenú `Support ESP32 internal EMAC controller`:
        * En `PHY interface`, selecciona `Reduced Media Independent Interface (RMII)`, ESP-IDF actualmente solo soporta el modo RMII.
        * En `RMII clock mode`, selecciona una de las fuentes de donde proviene el reloj RMII (50MHz): `Input RMII clock from external` o `Output RMII clock from internal`.
        * Si `Output RMII clock from internal` está habilitado, también debes establecer el número de GPIO que se usa para la salida del reloj RMII, en `RMII clock GPIO number`. En este caso, puedes establecer el número de GPIO en 16 o 17.
        * Si `Output RMII clock from GPIO0 (Experimental!)` también está habilitado, entonces no tienes otra opción que GPIO0 para la salida del reloj RMII.
        * En `Amount of Ethernet DMA Rx buffers` y `Amount of Ethernet DMA Tx buffers`, puedes establecer la cantidad de buffers DMA utilizados para Tx y Rx.
    * En el submenú `Support SPI to Ethernet Module`, selecciona el módulo SPI que usaste para este ejemplo. Actualmente ESP-IDF solo soporta `DM9051`, `W5500` y `KSZ8851SNL`.

## Solución de Problemas Comunes

* El panel de datos entre el MAC de ESP32 y el PHY necesita un reloj fijo de 50MHz para hacer la sincronización, también llamado reloj RMII. Puede ser proporcionado por un oscilador externo o generado desde el APLL interno. La integridad de la señal del reloj RMII es estricta, ¡así que mantén el trazado lo más corto posible!
* Si el reloj RMII se genera desde el APLL interno, entonces el APLL no se puede usar para otro propósito (por ejemplo, I2S).
* Si observas un comportamiento indefinido (por ejemplo, parpadeos en la LCD) de cualquier **dispositivo SPI** que funciona normalmente cuando Ethernet no está conectado a través del EMAC interno, necesitas ajustar la longitud de ráfaga DMA de EMAC (el DMA es un recurso compartido entre EMAC y el SPI). Lo mismo se aplica cuando observas corrupción de tramas Ethernet en la salida del módulo Ethernet SPI y usas una combinación de EMAC interno y módulo Ethernet SPI como interfaces de red. Para configurar la longitud de ráfaga DMA de EMAC, modifica la inicialización de Ethernet interna de la siguiente manera:

```c
esp32_emac_config.dma_burst_len = ETH_DMA_BURST_LEN_4; // or other appropriate value
```

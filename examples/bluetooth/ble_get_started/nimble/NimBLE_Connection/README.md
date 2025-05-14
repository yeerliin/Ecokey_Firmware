| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- |

# Ejemplo de Conexión NimBLE

## Descripción General

Este ejemplo se extiende del Ejemplo de Baliza NimBLE, y además introduce:

1. Cómo publicitar como un dispositivo periférico conectable
2. Cómo capturar eventos GAP y manejarlos
3. Cómo actualizar parámetros de conexión


Para probar esta demostración, instala *nRF Connect for Mobile* en tu teléfono. 

Por favor, consulta [Conexión BLE](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/get-started/ble-connection.html#:~:text=%E4%BE%8B%E7%A8%8B%E5%AE%9E%E8%B7%B5)
para obtener una introducción detallada del ejemplo y explicación del código.

## Pruébalo Tú Mismo

### Establecer el Objetivo

Antes de la configuración y compilación del proyecto, asegúrate de establecer el objetivo de chip correcto utilizando:

``` shell
idf.py set-target <chip_name>
```

Por ejemplo, si estás usando ESP32, entonces ingresa:

``` Shell
idf.py set-target esp32
```

### Compilar y Flashear

Ejecuta el siguiente comando para compilar, flashear y monitorear el proyecto.

``` Shell
idf.py -p <PUERTO> flash monitor
```

Por ejemplo, si el puerto serial correspondiente es `/dev/ttyACM0`, entonces sería:

``` Shell
idf.py -p /dev/ttyACM0 flash monitor
```

(Para salir del monitor serial, escribe ``Ctrl-]``.)

Consulta la [Guía de Inicio](https://idf.espressif.com/) para conocer los pasos completos para configurar y utilizar ESP-IDF para compilar proyectos.

## Código Explicado

### Descripción General

1. Inicializar LED, flash NVS, pila host NimBLE y servicio GAP; configurar la pila host NimBLE e iniciar el hilo de tarea del host NimBLE; esperar a que la pila host NimBLE se sincronice con el controlador BLE
2. Establecer datos de publicidad y respuesta de escaneo, luego configurar parámetros de publicidad e iniciar la publicidad
3. En el evento de conexión
    1. Encender el LED en la placa de desarrollo
    2. Imprimir descripciones de conexión
    3. Actualizar parámetros de conexión
4. En el evento de actualización de conexión
    1. Imprimir descripciones de conexión
5. En el evento de desconexión
    1. Apagar el LED en la placa de desarrollo
    2. Imprimir descripciones de conexión

### Punto de Entrada y En Sincronización de Pila

Por favor, consulta el Ejemplo de Baliza NimBLE para obtener detalles.

### Iniciar Publicidad

Hay algunas ligeras diferencias en este ejemplo en comparación con el Ejemplo de Baliza NimBLE. Primero, en este ejemplo estamos construyendo un periférico conectable, por lo que el modo de conexión se establece como conectable, es decir:

``` C
static void start_advertising(void) {
    ...

    /* Establecer modo no conectable y modo general descubrible para ser una baliza */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ...
}
```

Además, para demostrar la configuración de parámetros de publicidad, los parámetros de intervalo de publicidad se modifican a 500ms, y se muestran en la respuesta de escaneo. Ten en cuenta que la unidad del intervalo de publicidad es 0.625ms.

``` C
static void start_advertising(void) {
    ...

    /* Establecer intervalo de publicidad */
    rsp_fields.adv_itvl = BLE_GAP_ADV_ITVL_MS(500);
    rsp_fields.adv_itvl_is_present = 1;

    ...

    /* Establecer intervalo de publicidad */
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(500);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(510);

    ...
}
```

Y finalmente, al llamar a la API para iniciar la publicidad, se pasa una función de callback `gap_event_handler` como argumento para recibir eventos GAP. Hablaremos de esto en la siguiente sección.

``` C
static void start_advertising(void) {
    ...

    /* Iniciar publicidad */
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to start advertising, error code: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising started!");
}
```

### En Eventos GAP

Para mantenerlo simple, estamos interesados en 3 eventos GAP por el momento:

- `BLE_GAP_EVENT_CONNECT` - Evento de conexión
- `BLE_GAP_EVENT_DISCONNECT` - Evento de desconexión
- `BLE_GAP_EVENT_CONN_UPDATE` - Evento de actualización de conexión

#### Evento de Conexión

Cuando el dispositivo se conecta a un dispositivo par o falla una conexión, se pasará un evento de conexión a `gap_event_handler` por la pila host NimBLE. Primero verificaremos el estado de la conexión:

- Si tuvo éxito
    - Obtener el descriptor de conexión por el identificador de conexión e imprimirlo
    - Encender el LED
    - Intentar actualizar los parámetros de conexión
- Si falló
    - Reiniciar la publicidad

``` C
/* Evento de conexión */
static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    /* Variables locales */
    int rc = 0;
    struct ble_gap_conn_desc desc;

    /* Manejar diferentes eventos GAP */
    switch (event->type) {

    /* Evento de conexión */
    case BLE_GAP_EVENT_CONNECT:
        /* Se estableció una nueva conexión o falló un intento de conexión. */
        ESP_LOGI(TAG, "connection %s; status=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);

        /* Conexión exitosa */
        if (event->connect.status == 0) {
            /* Verificar el identificador de conexión */
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc != 0) {
                ESP_LOGE(TAG,
                         "failed to find connection by handle, error code: %d",
                         rc);
                return rc;
            }

            /* Imprimir el descriptor de conexión y encender el LED */
            print_conn_desc(&desc);
            led_on();

            /* Intentar actualizar los parámetros de conexión */
            struct ble_gap_upd_params params = {.itvl_min = desc.conn_itvl,
                                                .itvl_max = desc.conn_itvl,
                                                .latency = 3,
                                                .supervision_timeout =
                                                    desc.supervision_timeout};
            rc = ble_gap_update_params(event->connect.conn_handle, &params);
            if (rc != 0) {
                ESP_LOGE(
                    TAG,
                    "failed to update connection parameters, error code: %d",
                    rc);
                return rc;
            }
        }
        /* Falló la conexión, reiniciar publicidad */
        else {
            start_advertising();
        }
        return rc;

    ...
    }
}
```

#### Evento de Desconexión

En el evento de desconexión, simplemente:

1. Imprimimos la razón de la desconexión y el descriptor de conexión
2. Apagamos el LED
3. Reiniciamos la publicidad

``` C
static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    ...

    /* Evento de desconexión */
    case BLE_GAP_EVENT_DISCONNECT:
        /* Se terminó una conexión, imprimir el descriptor de conexión */
        ESP_LOGI(TAG, "disconnected from peer; reason=%d",
                 event->disconnect.reason);

        /* Apagar el LED */
        led_off();

        /* Reiniciar publicidad */
        start_advertising();
        return rc;

    ...
}
```

#### Evento de Actualización de Conexión

En el evento de actualización de conexión, la operación también es muy simple:

1. Imprimir el estado de la conexión
2. Obtener el descriptor de conexión por el identificador de conexión e imprimirlo

``` C
static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    ...

    /* Evento de actualización de parámetros de conexión */
    case BLE_GAP_EVENT_CONN_UPDATE:
        /* El dispositivo central ha actualizado los parámetros de conexión. */
        ESP_LOGI(TAG, "connection updated; status=%d",
                    event->conn_update.status);

        /* Imprimir el descriptor de conexión */
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        if (rc != 0) {
            ESP_LOGE(TAG, "failed to find connection by handle, error code: %d",
                        rc);
            return rc;
        }
        print_conn_desc(&desc);
        return rc;
        
    ...
}
```

## Observación

Si todo va bien, además de lo que hemos visto en el ejemplo de Baliza NimBLE, deberías poder ver el LED encendido cuando el dispositivo está conectado, y ver el LED apagado al desconectarse.

## Solución de Problemas

Para cualquier consulta técnica, por favor abre un [issue](https://github.com/espressif/esp-idf/issues) en GitHub. Te responderemos lo antes posible.

| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- |

# Ejemplo de Servidor GATT NimBLE

## Descripción General

Este ejemplo se extiende del Ejemplo de Conexión NimBLE, y además introduce:

1. Cómo implementar un servidor GATT utilizando la tabla de servicios GATT
2. Cómo manejar solicitudes de acceso a características
    1. Acceso de escritura demostrado mediante el control de LED
    2. Acceso de lectura e indicación demostrado mediante la medición de frecuencia cardíaca (simulada)

Para probar esta demostración, instala *nRF Connect for Mobile* en tu teléfono. 

Por favor, consulta [Introducción a BLE](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/get-started/ble-introduction.html#:~:text=%E4%BE%8B%E7%A8%8B%E5%AE%9E%E8%B7%B5)
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

1. Inicialización
    1. Inicializar LED, flash NVS, pila host NimBLE, servicio GAP
    2. Inicializar servicio GATT y agregar servicios a la cola de registro
    3. Configurar la pila host NimBLE e iniciar el hilo de tarea del host NimBLE, los servicios GATT se registrarán automáticamente cuando se inicie la pila host NimBLE
    4. Iniciar el hilo de tarea de actualización de frecuencia cardíaca
2. Esperar a que la pila host NimBLE se sincronice con el controlador BLE y comience a publicitar; esperar a que lleguen eventos de conexión
3. Después de establecer la conexión, esperar a que lleguen eventos de acceso a las características GATT
    1. En el evento de escritura de LED, encender o apagar el LED en consecuencia
    2. En el evento de lectura de frecuencia cardíaca, enviar el valor actual de medición de frecuencia cardíaca
    3. En el evento de indicación de frecuencia cardíaca, habilitar la indicación de frecuencia cardíaca

### Punto de Entrada

En este ejemplo, llamamos a la función GATT `gatt_svr_init` para inicializar el servidor GATT en `app_main` antes de la configuración del host NimBLE. Esta es una función personalizada definida en `gatt_svc.c`, y básicamente solo llamamos a la API de inicialización del servicio GATT y agregamos servicios a la cola de registro.

Y hay otro código agregado en `nimble_host_config_init`, que es:

``` C
static void nimble_host_config_init(void) {
    ...

    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;

    ...
}
```

Esa es la función de callback de registro del servidor GATT. En este caso, solo imprimirá alguna información de registro cuando se registren servicios, características o descriptores.

Luego, después de que se crea el hilo de tarea del host NimBLE, crearemos otra tarea definida en `heart_rate_task` para actualizar el valor simulado de la medición de frecuencia cardíaca y enviar indicación si está habilitado.

### Actualizaciones del Servicio GAP

`gap_event_handler` se actualiza con el control de LED eliminado y más ramas de manejo de eventos, en comparación con el Ejemplo de Conexión NimBLE, incluyendo:

- `BLE_GAP_EVENT_ADV_COMPLETE` - Evento de publicidad completa
- `BLE_GAP_EVENT_NOTIFY_TX` - Evento de notificación
- `BLE_GAP_EVENT_SUBSCRIBE` - Evento de suscripción
- `BLE_GAP_EVENT_MTU` - Evento de actualización de MTU

Los eventos `BLE_GAP_EVENT_ADV_COMPLETE` y `BLE_GAP_EVENT_MTU` no están realmente involucrados en este ejemplo, pero los incluimos como referencia. Los eventos `BLE_GAP_EVENT_NOTIFY_TX` y `BLE_GAP_EVENT_SUBSCRIBE` se discutirán en la siguiente sección.

### Tabla de Servicios GATT

Los servicios GATT se definen en una matriz de estructuras `ble_gatt_svc_def`, con un nombre de variable `gatt_svr_svcs` en esta demostración. Lo llamaremos como la tabla de servicios GATT en el siguiente contenido.

``` C
/* Servicio de frecuencia cardíaca */
static const ble_uuid16_t heart_rate_svc_uuid = BLE_UUID16_INIT(0x180D);

static uint8_t heart_rate_chr_val[2] = {0};
static uint16_t heart_rate_chr_val_handle;
static const ble_uuid16_t heart_rate_chr_uuid = BLE_UUID16_INIT(0x2A37);

static uint16_t heart_rate_chr_conn_handle = 0;
static bool heart_rate_chr_conn_handle_inited = false;
static bool heart_rate_ind_status = false;

/* Servicio de E/S de automatización */
static const ble_uuid16_t auto_io_svc_uuid = BLE_UUID16_INIT(0x1815);
static uint16_t led_chr_val_handle;
static const ble_uuid128_t led_chr_uuid =
    BLE_UUID128_INIT(0x23, 0xd1, 0xbc, 0xea, 0x5f, 0x78, 0x23, 0x15, 0xde, 0xef,
                     0x12, 0x12, 0x25, 0x15, 0x00, 0x00);

/* Tabla de servicios GATT */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    /* Servicio de frecuencia cardíaca */
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &heart_rate_svc_uuid.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {/* Característica de frecuencia cardíaca */
              .uuid = &heart_rate_chr_uuid.u,
              .access_cb = heart_rate_chr_access,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
              .val_handle = &heart_rate_chr_val_handle},
             {
                 0, /* No más características en este servicio. */
             }}},

    /* Servicio de E/S de automatización */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &auto_io_svc_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){/* Característica LED */
                                        {.uuid = &led_chr_uuid.u,
                                         .access_cb = led_chr_access,
                                         .flags = BLE_GATT_CHR_F_WRITE,
                                         .val_handle = &led_chr_val_handle},
                                        {0}},
    },
    
    {
        0, /* No más servicios. */
    },
};
```

En esta tabla, hay dos servicios GATT primarios definidos:

- Servicio de frecuencia cardíaca con un UUID de `0x180D`
- Servicio de E/S de automatización con un UUID de `0x1815`

#### Servicio de E/S de Automatización

Bajo el servicio de E/S de automatización, hay una característica LED, con un UUID específico del proveedor y permiso de solo escritura.

La característica está vinculada con la función de callback `led_chr_access`, en la que se captura el evento de acceso de escritura. El LED se encenderá o apagará según el valor de escritura, bastante sencillo.

``` C
static int led_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    /* Variables locales */
    int rc;

    /* Manejar eventos de acceso */
    /* Nota: La característica LED es solo de escritura */
    switch (ctxt->op) {

    /* Evento de escritura de característica */
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        /* Verificar el identificador de conexión */
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic write; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(TAG,
                     "characteristic write by nimble stack; attr_handle=%d",
                     attr_handle);
        }

        /* Verificar el identificador de atributo */
        if (attr_handle == led_chr_val_handle) {
            /* Verificar la longitud del búfer de acceso */
            if (ctxt->om->om_len == 1) {
                /* Encender o apagar el LED según el bit de operación */
                if (ctxt->om->om_data[0]) {
                    led_on();
                    ESP_LOGI(TAG, "led turned on!");
                } else {
                    led_off();
                    ESP_LOGI(TAG, "led turned off!");
                }
            } else {
                goto error;
            }
            return rc;
        }
        goto error;

    /* Evento desconocido */
    default:
        goto error;
    }

error:
    ESP_LOGE(TAG,
             "unexpected access operation to led characteristic, opcode: %d",
             ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}
```

#### Servicio de Frecuencia Cardíaca

Bajo el servicio de frecuencia cardíaca, hay una característica de medición de frecuencia cardíaca, con un UUID de `0x2A37` y permiso de acceso de lectura + indicación.

La característica está vinculada con la función de callback `heart_rate_chr_access`, en la que se captura el evento de acceso de lectura. Cabe mencionar que en la definición SIG, la medición de frecuencia cardíaca es una estructura de datos de múltiples bytes, con el primer byte indicando las banderas:

- Bit 0: Tipo de valor de frecuencia cardíaca
    - 0: El valor de frecuencia cardíaca es de tipo `uint8_t`
    - 1: El valor de frecuencia cardíaca es de tipo `uint16_t`
- Bit 1: Estado de contacto del sensor
- Bit 2: Contacto del sensor soportado
- Bit 3: Estado de energía gastada
- Bit 4: Estado del intervalo RR
- Bit 5-7: Reservado

y el resto de los bytes son campos de datos. En este caso, usamos el tipo `uint8_t` y deshabilitamos otras características, haciendo que el valor de la característica sea un array de 2 bytes. Así que cuando llega el evento de lectura de la característica, obtendremos el último valor de frecuencia cardíaca y lo enviaremos de vuelta al dispositivo par.

``` C
static int heart_rate_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) {
    /* Variables locales */
    int rc;

    /* Manejar eventos de acceso */
    /* Nota: La característica de frecuencia cardíaca es de solo lectura */
    switch (ctxt->op) {

    /* Evento de lectura de característica */
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* Verificar el identificador de conexión */
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(TAG, "characteristic read by nimble stack; attr_handle=%d",
                     attr_handle);
        }

        /* Verificar el identificador de atributo */
        if (attr_handle == heart_rate_chr_val_handle) {
            /* Actualizar el valor del búfer de acceso */
            heart_rate_chr_val[1] = get_heart_rate();
            rc = os_mbuf_append(ctxt->om, &heart_rate_chr_val,
                                sizeof(heart_rate_chr_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto error;

    /* Evento desconocido */
    default:
        goto error;
    }

error:
    ESP_LOGE(
        TAG,
        "unexpected access operation to heart rate characteristic, opcode: %d",
        ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}
```

El acceso de indicación, sin embargo, es un poco más complicado. Como se mencionó en *Actualizaciones del Servicio GAP*, manejaremos otros 2 eventos, a saber, `BLE_GAP_EVENT_NOTIFY_TX` y `BLE_GAP_EVENT_SUBSCRIBE` en `gap_event_handler`. En este caso, si el dispositivo par quiere habilitar la indicación de medición de frecuencia cardíaca, enviará una solicitud de suscripción al dispositivo local, y la solicitud será capturada como un evento de suscripción en `gap_event_handler`. Pero desde la perspectiva de la capa de software, el evento debe ser manejado en el servidor GATT, así que solo pasamos el evento al servidor GATT llamando a `gatt_svr_subscribe_cb`, como se demuestra en la demostración:

``` C
static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    ...

    /* Evento de suscripción */
    case BLE_GAP_EVENT_SUBSCRIBE:
        /* Imprimir información de suscripción en el registro */
        ESP_LOGI(TAG,
                    "subscribe event; conn_handle=%d attr_handle=%d "
                    "reason=%d prevn=%d curn=%d previ=%d curi=%d",
                    event->subscribe.conn_handle, event->subscribe.attr_handle,
                    event->subscribe.reason, event->subscribe.prev_notify,
                    event->subscribe.cur_notify, event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);

        /* Callback de evento de suscripción GATT */
        gatt_svr_subscribe_cb(event);
        return rc;
    
    ...
}
```

Luego verificaremos el identificador de conexión y el identificador de atributo, si el identificador de atributo coincide con `heart_rate_chr_val_chandle`, se actualizarán juntos `heart_rate_chr_conn_handle` y `heart_rate_ind_status`. 

``` C
void gatt_svr_subscribe_cb(struct ble_gap_event *event) {
    /* Verificar el identificador de conexión */
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    } else {
        ESP_LOGI(TAG, "subscribe by nimble stack; attr_handle=%d",
                 event->subscribe.attr_handle);
    }

    /* Verificar el identificador de atributo */
    if (event->subscribe.attr_handle == heart_rate_chr_val_handle) {
        /* Actualizar el estado de suscripción de frecuencia cardíaca */
        heart_rate_chr_conn_handle = event->subscribe.conn_handle;
        heart_rate_chr_conn_handle_inited = true;
        heart_rate_ind_status = event->subscribe.cur_indicate;
    }
}
```

Ten en cuenta que la indicación de medición de frecuencia cardíaca se maneja en `heart_rate_task` llamando a la función `send_heart_rate_indication` periódicamente. En realidad, esta función verificará el estado de indicación de frecuencia cardíaca y enviará la indicación en consecuencia. De esta manera, se implementa la indicación de frecuencia cardíaca.

``` C
void send_heart_rate_indication(void) {
    if (heart_rate_ind_status && heart_rate_chr_conn_handle_inited) {
        ble_gatts_indicate(heart_rate_chr_conn_handle,
                           heart_rate_chr_val_handle);
        ESP_LOGI(TAG, "heart rate indication sent!");
    }
}

static void heart_rate_task(void *param) {
    /* Entrada de tarea en el registro */
    ESP_LOGI(TAG, "heart rate task has been started!");

    /* Bucle infinito */
    while (1) {
        /* Actualizar el valor de frecuencia cardíaca cada 1 segundo */
        update_heart_rate();
        ESP_LOGI(TAG, "heart rate updated to %d", get_heart_rate());

        /* Enviar indicación de frecuencia cardíaca si está habilitada */
        send_heart_rate_indication();

        /* Dormir */
        vTaskDelay(HEART_RATE_TASK_PERIOD);
    }

    /* Limpiar al salir */
    vTaskDelete(NULL);
}
```

## Observación

Si todo va bien, deberías poder ver 4 servicios cuando te conectes a ESP32, incluyendo:

- Acceso Genérico
- Atributo Genérico
- Frecuencia Cardíaca
- E/S de Automatización

Haz clic en el Servicio de E/S de Automatización, deberías poder ver la característica LED. Haz clic en el botón de carga, deberías poder escribir el valor `ON` u `OFF`. Envíalo al dispositivo, el LED se encenderá o apagará siguiendo tu instrucción.

Haz clic en el Servicio de Frecuencia Cardíaca, deberías poder ver la característica de Medición de Frecuencia Cardíaca. Haz clic en el botón de descarga, deberías poder ver el último valor simulado de medición de frecuencia cardíaca, y debería ser consistente con lo que se muestra en la salida serial. Haz clic en el botón de suscripción, deberías poder ver el valor simulado de medición de frecuencia cardíaca actualizado cada segundo.

## Solución de Problemas

Para cualquier consulta técnica, por favor abre un [issue](https://github.com/espressif/esp-idf/issues) en GitHub. Te responderemos lo antes posible.

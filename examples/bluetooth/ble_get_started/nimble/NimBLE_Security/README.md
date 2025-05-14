| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- |

# Ejemplo de Seguridad NimBLE

## Descripción General

Este ejemplo se extiende del Ejemplo del Servidor GATT de NimBLE, y además introduce:

1. Cómo establecer una dirección privada aleatoria no resoluble para el dispositivo
2. Cómo solicitar el cifrado de la conexión desde el lado periférico en el acceso a las características
3. Cómo emparejar con un dispositivo par utilizando una clave de acceso de 6 dígitos generada aleatoriamente


Para probar esta demostración, instala *nRF Connect for Mobile* en tu teléfono. 

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

El siguiente es contenido adicional en comparación con el ejemplo del Servidor GATT de NimBLE.

1. El procedimiento de inicialización es generalmente similar al Ejemplo del Servidor GATT de NimBLE, pero inicializaremos el generador de números aleatorios al principio, y en `nimble_host_config_init` configuraremos el administrador de seguridad para habilitar características relacionadas
2. En la sincronización de la pila, se genera una dirección privada aleatoria no resoluble y se establece como la dirección del dispositivo
3. Los permisos de las características se modifican para requerir cifrado de conexión al acceder
4. Se agregan 3 ramas de eventos GAP más en `gap_event_handler` para manejar eventos relacionados con el cifrado

### Punto de Entrada

En la función `nimble_host_config_init`, vamos a habilitar algunas características del administrador de seguridad, incluyendo:

- Emparejamiento
- Protección contra ataques de intermediario
- Distribución de claves

Además, vamos a establecer la capacidad de E/S a `BLE_HS_IO_DISPLAY_ONLY`, ya que es posible imprimir la clave de acceso en la salida serial.

``` C
static void nimble_host_config_init(void) {
    ...

    /* Configuración del administrador de seguridad */
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ...
}
```

### Actualizaciones del Servidor GATT

Para la característica de frecuencia cardíaca y la característica LED, se agregan las banderas `BLE_GATT_CHR_F_READ_ENC` y `BLE_GATT_CHR_F_WRITE_ENC` respectivamente para requerir cifrado de conexión cuando el cliente GATT intenta acceder a la característica. Gracias a la pila del host NimBLE, el cifrado de la conexión se iniciará automáticamente al agregar estas banderas.

Sin embargo, la característica de frecuencia cardíaca también es indicable, y la pila del host NimBLE no ofrece una implementación para que el acceso de indicación requiera cifrado de conexión, por lo que debemos hacerlo nosotros mismos. Para el servidor GATT, simplemente verificamos el estado de seguridad de la conexión llamando a una función externa `is_connection_encrypted` en la función `send_heart_rate_indication` para determinar si se debe enviar la indicación. Esta función externa se define en la capa GAP, y hablaremos de ella en la sección *Actualizaciones del Manejador de Eventos GAP*.

``` C
void send_heart_rate_indication(void) {
    /* Comprobar si el identificador de conexión está inicializado */
    if (!heart_rate_chr_conn_handle_inited) {
        return;
    }

    /* Comprobar el estado de indicación y seguridad */
    if (heart_rate_ind_status &&
        is_connection_encrypted(heart_rate_chr_conn_handle)) {
        ble_gatts_indicate(heart_rate_chr_conn_handle,
                           heart_rate_chr_val_handle);
    }
}
```

### Dirección Aleatoria

En la siguiente función, podemos generar una dirección privada aleatoria no resoluble y establecerla como la dirección del dispositivo. La llamaremos en la función `adv_init` antes de asegurar la disponibilidad de la dirección.

``` C
static void set_random_addr(void) {
    /* Variables locales */
    int rc = 0;
    ble_addr_t addr;

    /* Generar nueva dirección privada no resoluble */
    rc = ble_hs_id_gen_rnd(0, &addr);
    assert(rc == 0);

    /* Establecer dirección */
    rc = ble_hs_id_set_rnd(addr.val);
    assert(rc == 0);
}
```

### Verificar el Estado de Cifrado de la Conexión

Por el identificador de conexión, podemos obtener el descriptor de conexión de la pila host de NimBLE, y hay una bandera que indica el estado de cifrado de la conexión, ver los siguientes códigos:

``` C
bool is_connection_encrypted(uint16_t conn_handle) {
    /* Variables locales */
    int rc = 0;
    struct ble_gap_conn_desc desc;

    /* Imprimir descriptor de conexión */
    rc = ble_gap_conn_find(conn_handle, &desc);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to find connection by handle, error code: %d",
                 rc);
        return false;
    }

    return desc.sec_state.encrypted;
}
```

### Actualizaciones del Manejador de Eventos GAP

Se agregaron 3 ramas más de eventos GAP en `gap_event_handler`, que son:

- `BLE_GAP_EVENT_ENC_CHANGE` - Evento de cambio de cifrado
- `BLE_GAP_EVENT_REPEAT_PAIRING` - Evento de emparejamiento repetido
- `BLE_GAP_EVENT_PASSKEY_ACTION` - Evento de acción de clave de acceso

En el evento de cambio de cifrado, vamos a imprimir el estado del cambio de cifrado en la salida.

``` C
/* Evento de cambio de cifrado */
case BLE_GAP_EVENT_ENC_CHANGE:
    /* El cifrado ha sido habilitado o deshabilitado para esta conexión. */
    if (event->enc_change.status == 0) {
        ESP_LOGI(TAG, "connection encrypted!");
    } else {
        ESP_LOGE(TAG, "connection encryption failed, status: %d",
                    event->enc_change.status);
    }
    return rc;
```

En el evento de emparejamiento repetido, para simplificarlo, simplemente eliminaremos el vínculo antiguo y repetiremos el emparejamiento.

``` C
/* Evento de emparejamiento repetido */
case BLE_GAP_EVENT_REPEAT_PAIRING:
    /* Eliminar el vínculo antiguo */
    rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to find connection, error code %d", rc);
        return rc;
    }
    ble_store_util_delete_peer(&desc.peer_id_addr);

    /* Devolver BLE_GAP_REPEAT_PAIRING_RETRY para indicar que el host debe
        * continuar con la operación de emparejamiento */
    ESP_LOGI(TAG, "repairing...");
    return BLE_GAP_REPEAT_PAIRING_RETRY;
```

En el evento de acción de clave de acceso, se genera una clave de acceso aleatoria de 6 dígitos, y se supone que debes ingresar la misma clave de acceso al emparejar. Si la entrada es consistente con la clave de acceso generada, deberías poder vincularte con el dispositivo.

``` C
/* Evento de acción de clave de acceso */
case BLE_GAP_EVENT_PASSKEY_ACTION:
    /* Acción de visualización */
    if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
        /* Generar clave de acceso */
        struct ble_sm_io pkey = {0};
        pkey.action = event->passkey.params.action;
        pkey.passkey = 100000 + esp_random() % 900000;
        ESP_LOGI(TAG, "enter passkey %" PRIu32 " on the peer side",
                    pkey.passkey);
        rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
        if (rc != 0) {
            ESP_LOGE(TAG,
                        "failed to inject security manager io, error code: %d",
                        rc);
            return rc;
        }
    }
    return rc;
```

## Observación

Si todo va bien, se requerirá emparejamiento cuando intentes acceder a cualquiera de las características, es decir, leer o indicar la característica de frecuencia cardíaca, o escribir la característica LED.

## Solución de Problemas

Para cualquier consulta técnica, por favor abre un [issue](https://github.com/espressif/esp-idf/issues) en GitHub. Te responderemos lo antes posible.

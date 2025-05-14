| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- |

# Ejemplo de Baliza NimBLE

## Descripción General

Este es un ejemplo bastante simple, que tiene como objetivo introducir:

1. Cómo inicializar la pila NimBLE
2. Cómo configurar los datos de publicidad y respuesta de escaneo
3. Cómo iniciar la publicidad como una baliza no conectable


Para probar esta demostración, instala *nRF Connect for Mobile* en tu teléfono. 

Por favor, consulta [Descubrimiento de Dispositivos BLE](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/get-started/ble-device-discovery.html#:~:text=%E4%BE%8B%E7%A8%8B%E5%AE%9E%E8%B7%B5)
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

1. Inicializar el flash NVS, la pila host NimBLE y el servicio GAP; configurar la pila host NimBLE e iniciar el hilo de tarea del host NimBLE
2. Esperar a que la pila host NimBLE se sincronice con el controlador BLE
3. Establecer datos de publicidad y respuesta de escaneo, luego configurar parámetros de publicidad e iniciar la publicidad

### Punto de Entrada

`app_main` en `main.c` es el punto de entrada de todas las aplicaciones ESP32. En general, la inicialización de la aplicación debe hacerse aquí.

Primero, llama a la función `nvs_flash_init` para inicializar el flash NVS, que es la dependencia para que el módulo BLE almacene configuraciones.

``` C
void app_main(void) {
    /* Variables locales */
    int rc;
    esp_err_t ret;

    /* Inicialización del flash NVS */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return;
    }

    ...
}
```

Luego, llama a la función `nimble_port_init` para inicializar la pila host NimBLE.

``` C
void app_main(void) {
    ...

    /* Inicialización de la pila host NimBLE */
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ",
                 ret);
        return;
    }

    ...
}
```

Después de eso, llama a `gap_init` definido en `gap.c`. Inicializaremos el servicio GAP, estableceremos el nombre y la apariencia del dispositivo GAP en esta función.

``` C
int gap_init(void) {
    /* Variables locales */
    int rc = 0;

    /* Inicializar servicio GAP */
    ble_svc_gap_init();

    /* Establecer nombre del dispositivo GAP */
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device name to %s, error code: %d",
                 DEVICE_NAME, rc);
        return rc;
    }

    /* Establecer apariencia del dispositivo GAP */
    rc = ble_svc_gap_device_appearance_set(BLE_GAP_APPEARANCE_GENERIC_TAG);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device appearance, error code: %d", rc);
        return rc;
    }
    return rc;
}

void app_main(void) {
    ...

    /* Inicialización del servicio GAP */
    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return;
    }

    ...
}
```

Y necesitamos configurar algunas funciones de callback para que la pila host NimBLE las llame y almacene las configuraciones en `nimble_host_config_init` en `main.c`.

``` C
static void nimble_host_config_init(void) {
    /* Establecer callbacks del host */
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Almacenar configuración del host */
    ble_store_config_init();
}

void app_main(void) {
    ...

    /* Inicialización de la configuración del host NimBLE */
    nimble_host_config_init();

    ...
}
```

Hasta ahora, la inicialización ha sido completada. Podemos llamar a `xTaskCreate` para crear el hilo `nimble_host_task`, y dejar que la pila host NimBLE se ejecute en segundo plano.

``` C
static void nimble_host_task(void *param) {
    /* Registro de entrada de tarea */
    ESP_LOGI(TAG, "nimble host task has been started!");

    /* Esta función no retornará hasta que se ejecute nimble_port_stop() */
    nimble_port_run();

    /* Limpiar al salir */
    vTaskDelete(NULL);
}

void app_main(void) {
    ...

    /* Iniciar el hilo de tarea del host NimBLE y retornar */
    xTaskCreate(nimble_host_task, "NimBLE Host", 4*1024, NULL, 5, NULL);
    return;
}
```

### En Sincronización de Pila

Una vez que la pila host NimBLE se sincroniza con el controlador BLE, `on_stack_sync` en `gap.c` será llamada por la pila host NimBLE, que ha sido configurada en `nimble_host_config_init`.

En esta función, llamaremos a la función `adv_init` para pedirle a la pila host NimBLE que verifique si la dirección MAC del dispositivo está disponible mediante las funciones `ble_hs_util_ensure_addr` y `ble_hs_id_infer_auto`. Si es así, copiaremos la dirección e intentaremos iniciar la publicidad llamando a `start_advertising` en el mismo archivo fuente.

``` C
static void on_stack_sync(void) {
    /* En la sincronización de la pila, realizar la inicialización de la publicidad */
    adv_init();
}

void adv_init(void) {
    /* Variables locales */
    int rc = 0;
    char addr_str[18] = {0};

    /* Asegurarse de que tengamos una dirección BT de identidad adecuada establecida */
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "device does not have any available bt address!");
        return;
    }

    /* Determinar la dirección BT a usar durante la publicidad */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to infer address type, error code: %d", rc);
        return;
    }

    /* Copiar la dirección del dispositivo a addr_val */
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to copy device address, error code: %d", rc);
        return;
    }
    format_addr(addr_str, addr_val);
    ESP_LOGI(TAG, "device address: %s", addr_str);

    /* Iniciar publicidad. */
    start_advertising();
}
```

### Iniciar Publicidad

Como un dispositivo baliza, vamos a iniciar la publicidad y enviar respuesta de escaneo si se recibe una solicitud de escaneo. Para que esto suceda, necesitamos establecer los datos de publicidad y respuesta de escaneo antes de que comience la publicidad. Así que lo siguiente es lo que hacemos:

1. Inicializar las estructuras de campos de publicidad y respuesta de escaneo `adv_fields` y `rsp_fields`, así como la estructura de parámetros de publicidad `adv_params`

``` C
static void start_advertising(void) {
    /* Variables locales */
    int rc = 0;
    const char *name;
    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    ...
}
```

2. Establecer banderas de publicidad, nombre del dispositivo, potencia de transmisión, apariencia y rol LE en `adv_fields`, y llamar a `ble_gap_adv_set_fields`
    1. Para las banderas de publicidad, `BLE_HS_ADV_F_DISC_GEN` significa que la publicidad es descubrible generalmente, y `BLE_HS_ADV_F_BREDR_UNSUP` significa que solo se admite BLE (BR/EDR se refiere a Bluetooth Clásico)
    2. Para la apariencia, se utiliza para indicar al escáner qué aspecto tiene; usamos `BLE_GAP_APPEARANCE_GENERIC_TAG` aquí para que nuestro dispositivo sea identificado como una etiqueta

``` C
static void start_advertising(void) {
    ...

    /* Establecer banderas de publicidad */
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* Establecer nombre del dispositivo */
    name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;

    /* Establecer potencia de transmisión del dispositivo */
    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_fields.tx_pwr_lvl_is_present = 1;

    /* Establecer apariencia del dispositivo */
    adv_fields.appearance = BLE_GAP_APPEARANCE_GENERIC_TAG;
    adv_fields.appearance_is_present = 1;

    /* Establecer rol LE del dispositivo */
    adv_fields.le_role = BLE_GAP_LE_ROLE_PERIPHERAL;
    adv_fields.le_role_is_present = 1;

    /* Establecer campos de publicidad */
    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set advertising data, error code: %d", rc);
        return;
    }

    ...
}
```

3. Establecer dirección del dispositivo y URI en `rsp_fields`, y llamar a `ble_gap_adv_rsp_set_fields`
    1. Ya que los `AdvData` en el paquete de publicidad **no deben ser más largos que 31 bytes**, la información adicional debe colocarse en el paquete de respuesta de escaneo
    2. Colocamos el enlace al sitio web oficial de espressif en el campo URI

``` C
static void start_advertising(void) {
    ...

    /* Establecer dirección del dispositivo */
    rsp_fields.device_addr = addr_val;
    rsp_fields.device_addr_type = own_addr_type;
    rsp_fields.device_addr_is_present = 1;

    /* Establecer URI */
    rsp_fields.uri = esp_uri;
    rsp_fields.uri_len = sizeof(esp_uri);

    /* Establecer campos de respuesta de escaneo */
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set scan response data, error code: %d", rc);
        return;
    }

    ...
}
```

4. Establecer el modo de publicidad y el modo descubrible a no conectable y general-descubrible respectivamente en `adv_params`, y finalmente, iniciar la publicidad llamando a `ble_gap_adv_start`

``` C
static void start_advertising(void) {
    ...

    /* Establecer modo no conectable y modo general descubrible para ser una baliza */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    /* Iniciar publicidad */
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to start advertising, error code: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising started!");
}
```

### Observación

Si todo va bien, deberías poder ver `NimBLE_Beacon` en un dispositivo escáner BLE, transmitiendo mucha información incluida una URI de "https://espressif.com" (El sitio web oficial de espressif), que es exactamente lo que esperamos.

## Solución de Problemas

Para cualquier consulta técnica, por favor abre un [issue](https://github.com/espressif/esp-idf/issues) en GitHub. Te responderemos lo antes posible.

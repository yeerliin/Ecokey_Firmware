/* 
   Este es un ejemplo de Provisioning WiFi (configuración de red WiFi)
   
   Provisioning significa: permitir que un usuario configure el ESP32 para
   conectarse a una red WiFi sin tener que programar las credenciales directamente
   en el código.
*/

// =========== INCLUSIÓN DE BIBLIOTECAS ===========

// Bibliotecas estándar de C
#include <stdio.h>       // Para funciones de entrada/salida estándar
#include <string.h>      // Para manipulación de cadenas de texto

// Bibliotecas del sistema operativo FreeRTOS
#include <freertos/FreeRTOS.h>    // Sistema operativo en tiempo real
#include <freertos/task.h>        // Para gestionar tareas
#include <freertos/event_groups.h> // Para sincronizar tareas mediante eventos

// Bibliotecas específicas de ESP-IDF
#include <esp_log.h>     // Para imprimir mensajes de depuración
#include <esp_wifi.h>    // Para controlar el WiFi
#include <esp_event.h>   // Para manejar eventos del sistema
#include <nvs_flash.h>   // Para almacenar datos en memoria no volátil

// Biblioteca de provisioning WiFi
#include <wifi_provisioning/manager.h>

// Incluye bibliotecas específicas según el método de provisioning seleccionado
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
#include <wifi_provisioning/scheme_ble.h>  // Para provisioning mediante Bluetooth LE
#endif

#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
#include <wifi_provisioning/scheme_softap.h> // Para provisioning mediante punto de acceso WiFi
#endif

#include "qrcode.h"  // Para generar códigos QR en la consola

// =========== DEFINICIONES GLOBALES ===========

// Etiqueta para mensajes de log
static const char *TAG = "app";

// Configuración de seguridad para la versión 2 del protocolo de provisioning
#if CONFIG_EXAMPLE_PROV_SECURITY_VERSION_2
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
// Credenciales de desarrollo (¡no usar en producción!)
#define EXAMPLE_PROV_SEC2_USERNAME          "wifiprov"
#define EXAMPLE_PROV_SEC2_PWD               "abcd1234"

/* 
   Estos valores de salt y verificador se generaron para el usuario "wifiprov" y contraseña "abcd1234"
   IMPORTANTE: En producción, estos valores deben ser únicos para cada dispositivo
   y deben venir de una partición de fabricación.
*/
static const char sec2_salt[] = {
    // ... (datos binarios del salt) ...
    0x03, 0x6e, 0xe0, 0xc7, 0xbc, 0xb9, 0xed, 0xa8, 0x4c, 0x9e, 0xac, 0x97, 0xd9, 0x3d, 0xec, 0xf4
};

static const char sec2_verifier[] = {
    // ... (datos binarios del verificador) ...
    // Estos datos son necesarios para la autenticación segura
    0x7c, 0x7c, 0x85, 0x47, 0x65, 0x08, 0x94, 0x6d, 0xd6, 0x36, 0xaf, 0x37, 0xd7, 0xe8, 0x91, 0x43,
    // ... (más datos del verificador) ...
};
#endif

// Funciones para obtener el salt y el verificador de seguridad
static esp_err_t example_get_sec2_salt(const char **salt, uint16_t *salt_len) {
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
    // Modo desarrollo: usando salt codificado
    ESP_LOGI(TAG, "Modo desarrollo: usando salt codificado");
    *salt = sec2_salt;
    *salt_len = sizeof(sec2_salt);
    return ESP_OK;
#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
    // Modo producción: debería implementarse para obtener valores únicos
    ESP_LOGE(TAG, "¡No implementado!");
    return ESP_FAIL;
#endif
}

static esp_err_t example_get_sec2_verifier(const char **verifier, uint16_t *verifier_len) {
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
    // Modo desarrollo: usando verificador codificado
    ESP_LOGI(TAG, "Modo desarrollo: usando verificador codificado");
    *verifier = sec2_verifier;
    *verifier_len = sizeof(sec2_verifier);
    return ESP_OK;
#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
    ESP_LOGE(TAG, "¡No implementado!");
    return ESP_FAIL;
#endif
}
#endif

// Grupo de eventos para señalizar la conexión WiFi
const int WIFI_CONNECTED_EVENT = BIT0;  // Define un bit para el evento de conexión
static EventGroupHandle_t wifi_event_group;  // Grupo de eventos para sincronización

// Constantes para la generación del código QR
#define PROV_QR_VERSION         "v1"
#define PROV_TRANSPORT_SOFTAP   "softap"
#define PROV_TRANSPORT_BLE      "ble"
#define QRCODE_BASE_URL         "https://espressif.github.io/esp-jumpstart/qrcode.html"

// =========== FUNCIONES DE MANEJO DE EVENTOS ===========

/* 
   Esta función se ejecuta automáticamente cuando ocurren eventos en el sistema,
   como conexión WiFi, progreso del provisioning, etc.
*/
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    // Eventos relacionados con el provisioning WiFi
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                // Cuando comienza el proceso de provisioning
                ESP_LOGI(TAG, "Provisioning iniciado");
                break;
            case WIFI_PROV_CRED_RECV: {
                // Cuando se reciben las credenciales WiFi desde la app
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG, "Credenciales WiFi recibidas"
                         "\n\tSSID     : %s\n\tContraseña : %s",
                         (const char *) wifi_sta_cfg->ssid,
                         (const char *) wifi_sta_cfg->password);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                // Cuando falla la conexión con las credenciales proporcionadas
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG, "¡Provisioning fallido!\n\tRazón : %s"
                         "\n\tPor favor, restablece a valores de fábrica e intenta el provisioning nuevamente",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                         "Autenticación WiFi fallida" : "Red WiFi no encontrada");
#ifdef CONFIG_EXAMPLE_RESET_PROV_MGR_ON_FAILURE
                // Reinicia la máquina de estados si el provisioning falla
                wifi_prov_mgr_reset_sm_state_on_failure();
#endif
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                // Cuando la conexión con las credenciales es exitosa
                ESP_LOGI(TAG, "Provisioning exitoso");
                break;
            case WIFI_PROV_END:
                // Cuando finaliza el proceso de provisioning
                /* Des-inicializa el gestor cuando finaliza el provisioning */
                wifi_prov_mgr_deinit();
                break;
            default:
                break;
        }
    } 
    // Eventos relacionados con el WiFi
    else if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                // Cuando se inicia el modo estación WiFi
                esp_wifi_connect();  // Intenta conectarse a la red configurada
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                // Cuando se pierde la conexión WiFi
                ESP_LOGI(TAG, "Desconectado. Reconectando al AP...");
                esp_wifi_connect();  // Intenta reconectarse
                break;
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
            // Eventos específicos del modo punto de acceso
            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "SoftAP: ¡Cliente conectado!");
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(TAG, "SoftAP: ¡Cliente desconectado!");
                break;
#endif
            default:
                break;
        }
    } 
    // Evento cuando se obtiene una dirección IP
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Conectado con dirección IP:" IPSTR, IP2STR(&event->ip_info.ip));
        /* Señala a la aplicación principal que continúe la ejecución */
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
    // Eventos específicos del transporte BLE
    } else if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT) {
        switch (event_id) {
            case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
                ESP_LOGI(TAG, "Transporte BLE: ¡Conectado!");
                break;
            case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
                ESP_LOGI(TAG, "Transporte BLE: ¡Desconectado!");
                break;
            default:
                break;
        }
#endif
    } 
    // Eventos de seguridad de la sesión de comunicación
    else if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT) {
        switch (event_id) {
            case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
                ESP_LOGI(TAG, "¡Sesión segura establecida!");
                break;
            case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
                ESP_LOGE(TAG, "¡Parámetros de seguridad inválidos para establecer sesión segura!");
                break;
            case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
                ESP_LOGE(TAG, "¡Usuario y/o contraseña incorrectos para establecer sesión segura!");
                break;
            default:
                break;
        }
    }
}

// =========== FUNCIONES AUXILIARES ===========

// Inicializa el WiFi en modo estación (cliente)
static void wifi_init_sta(void)
{
    /* Inicia el WiFi en modo estación */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// Genera un nombre de servicio único basado en la dirección MAC
static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];  // La dirección MAC tiene 6 bytes
    const char *ssid_prefix = "PROV_";  // Prefijo para el nombre del servicio
    // Obtiene la dirección MAC del dispositivo
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    // Formatea el nombre con el prefijo y los últimos 3 bytes de la MAC
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

/* 
   Manejador para el punto final de provisioning personalizado.
   Permite a la app enviar y recibir datos personalizados durante el provisioning.
*/
esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                   uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    // Si hay datos entrantes, los muestra
    if (inbuf) {
        ESP_LOGI(TAG, "Datos recibidos: %.*s", inlen, (char *)inbuf);
    }
    // Prepara una respuesta
    char response[] = "SUCCESS";
    // Duplica la respuesta en memoria para devolverla
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL) {
        ESP_LOGE(TAG, "Sistema sin memoria");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1; // +1 para el byte NULL de terminación

    return ESP_OK;
}

// Genera y muestra un código QR para facilitar el provisioning
static void wifi_prov_print_qr(const char *name, const char *username, const char *pop, const char *transport)
{
    if (!name || !transport) {
        ESP_LOGW(TAG, "No se puede generar el código QR. Faltan datos.");
        return;
    }
    // Prepara la carga útil del código QR
    char payload[150] = {0};
    if (pop) {
#if CONFIG_EXAMPLE_PROV_SECURITY_VERSION_1
        // Formato para seguridad versión 1
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\"" \
                    ",\"pop\":\"%s\",\"transport\":\"%s\"}",
                    PROV_QR_VERSION, name, pop, transport);
#elif CONFIG_EXAMPLE_PROV_SECURITY_VERSION_2
        // Formato para seguridad versión 2
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\"" \
                    ",\"username\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
                    PROV_QR_VERSION, name, username, pop, transport);
#endif
    } else {
        // Formato sin prueba de posesión (menos seguro)
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\"" \
                    ",\"transport\":\"%s\"}",
                    PROV_QR_VERSION, name, transport);
    }
#ifdef CONFIG_EXAMPLE_PROV_SHOW_QR
    // Muestra el código QR en la consola
    ESP_LOGI(TAG, "Escanea este código QR desde la aplicación de provisioning.");
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    esp_qrcode_generate(&cfg, payload);
#endif
    // También muestra un enlace para abrir en el navegador si el QR no es visible
    ESP_LOGI(TAG, "Si el código QR no es visible, copia y pega esta URL en un navegador.\n%s?data=%s", QRCODE_BASE_URL, payload);
}

// Callback opcional para eventos de provisioning (si está habilitado)
#ifdef CONFIG_EXAMPLE_PROV_ENABLE_APP_CALLBACK
void wifi_prov_app_callback(void *user_data, wifi_prov_cb_event_t event, void *event_data)
{
    /**
     * Este es un callback bloqueante, cualquier configuración que deba establecerse cuando
     * se dispara un evento particular de provisioning puede configurarse aquí.
    */
    switch (event) {
        case WIFI_PROV_SET_STA_CONFIG: {
            /**
             * Las configuraciones WiFi pueden establecerse aquí antes de que se habilite
             * el WiFi en modo estación.
            */
            wifi_config_t *wifi_config = (wifi_config_t*)event_data;
            (void) wifi_config;  // Evita advertencia de variable no utilizada
            break;
        }
        default:
            break;
    }
}

// Estructura de manejador de eventos para callbacks de provisioning
const wifi_prov_event_handler_t wifi_prov_event_handler = {
    .event_cb = wifi_prov_app_callback,
    .user_data = NULL,
};
#endif

// =========== FUNCIÓN PRINCIPAL ===========

void app_main(void)
{
    // ===== INICIALIZACIÓN BÁSICA =====
    
    /* Inicializa la partición NVS (almacenamiento no volátil) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Si la partición NVS está corrupta o incompatible,
         * se borra y se vuelve a inicializar */
        ESP_ERROR_CHECK(nvs_flash_erase());
        /* Reintenta la inicialización */
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Inicializa la pila TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Inicializa el bucle de eventos */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();

    /* Registra los manejadores de eventos para WiFi, IP y Provisioning */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
#endif
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    /* Inicializa el WiFi incluyendo la interfaz de red con configuración predeterminada */
    esp_netif_create_default_wifi_sta();
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
    esp_netif_create_default_wifi_ap();
#endif
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // ===== CONFIGURACIÓN DEL GESTOR DE PROVISIONING =====
    
    /* Configuración para el gestor de provisioning */
    wifi_prov_mgr_config_t config = {
#ifdef CONFIG_EXAMPLE_RESET_PROV_MGR_ON_FAILURE
        .wifi_prov_conn_cfg = {
           .wifi_conn_attempts = CONFIG_EXAMPLE_PROV_MGR_CONNECTION_CNT,
        },
#endif
        /* ¿Qué esquema de provisioning queremos?
         * wifi_prov_scheme_softap o wifi_prov_scheme_ble */
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
        .scheme = wifi_prov_scheme_ble,
#endif
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
        .scheme = wifi_prov_scheme_softap,
#endif
#ifdef CONFIG_EXAMPLE_PROV_ENABLE_APP_CALLBACK
        .app_event_handler = wifi_prov_event_handler,
#endif

        /* Manejador de eventos específico del esquema elegido.
         * Permite liberar memoria de BT/BLE cuando ya no se necesita */
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
#endif
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
#endif
    };

    /* Inicializa el gestor de provisioning con los parámetros de configuración */
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    // ===== VERIFICACIÓN DEL ESTADO DE PROVISIONING =====
    
    bool provisioned = false;
#ifdef CONFIG_EXAMPLE_RESET_PROVISIONED
    // Opción para resetear el estado de provisioning
    wifi_prov_mgr_reset_provisioning();
#else
    /* Verificamos si el dispositivo ya está aprovisionado */
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
#endif

    // ===== PROCESO DE PROVISIONING =====
    
    /* Si el dispositivo no está aprovisionado, iniciamos el servicio de provisioning */
    if (!provisioned) {
        ESP_LOGI(TAG, "Iniciando provisioning");

        /* Generamos un nombre de servicio único basado en la MAC */
        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));

#ifdef CONFIG_EXAMPLE_PROV_SECURITY_VERSION_1
        /* Nivel de seguridad para el provisioning (0, 1, 2):
         * - WIFI_PROV_SECURITY_0: comunicación en texto plano
         * - WIFI_PROV_SECURITY_1: comunicación segura con intercambio de claves X25519 y prueba de posesión
         * - WIFI_PROV_SECURITY_2: autenticación SRP6a y cifrado AES-GCM
         */
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

        /* Prueba de posesión (ignorada si se selecciona Security 0):
         * - cadena de texto de longitud > 0
         * - NULL si no se usa
         */
        const char *pop = "abcd1234";

        /* Estructura para pasar parámetros de seguridad
         * para protocomm security 1.
         */
        wifi_prov_security1_params_t *sec_params = pop;

        const char *username = NULL;

#elif CONFIG_EXAMPLE_PROV_SECURITY_VERSION_2
        wifi_prov_security_t security = WIFI_PROV_SECURITY_2;
        /* El nombre de usuario debe ser el mismo que se usó para generar el salt y el verificador */

#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
        /* En modo desarrollo, guardamos la contraseña para generar el código QR
         * En producción, este campo no debe almacenarse en el dispositivo */
        const char *username = EXAMPLE_PROV_SEC2_USERNAME;
        const char *pop = EXAMPLE_PROV_SEC2_PWD;
#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
        /* En producción, el usuario y contraseña no deben estar en el firmware,
         * deben proporcionarse al usuario por otros medios, como una etiqueta QR */
        const char *username = NULL;
        const char *pop = NULL;
#endif
        /* Estructura para parámetros de seguridad versión 2 */
        wifi_prov_security2_params_t sec2_params = {};

        ESP_ERROR_CHECK(example_get_sec2_salt(&sec2_params.salt, &sec2_params.salt_len));
        ESP_ERROR_CHECK(example_get_sec2_verifier(&sec2_params.verifier, &sec2_params.verifier_len));

        wifi_prov_security2_params_t *sec_params = &sec2_params;
#endif
        /* Clave de servicio (puede ser NULL)
         * - En SoftAP: contraseña WiFi (mínimo 8 caracteres)
         * - En BLE: se ignora
         */
        const char *service_key = NULL;

#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
        /* Para BLE, configuramos un UUID personalizado de 128 bits
         * Este UUID se incluirá en la publicidad BLE y será el servicio GATT primario
         * que ofrece los puntos finales de provisioning como características GATT */
        uint8_t custom_service_uuid[] = {
            /* LSB <-----------------------------------
             * -----------------------------------> MSB */
            0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
            0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
        };

        /* Si tu compilación falla con errores de enlazador en este punto,
         * es posible que hayas olvidado habilitar la pila BT o la configuración
         * BTDM BLE en el SDK */
        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);
#endif

        /* Un punto final opcional que las aplicaciones pueden crear si esperan
         * recibir datos personalizados durante el flujo de aprovisionamiento.
         * El nombre del punto final puede ser cualquier cosa de tu elección.
         * Esta llamada debe hacerse antes de iniciar el provisionamiento.
         */
        wifi_prov_mgr_endpoint_create("custom-data");

        /* No detener y des-inicializar el provisionamiento incluso después del éxito,
         * para poder reiniciarlo más tarde. */
#ifdef CONFIG_EXAMPLE_REPROVISIONING
        wifi_prov_mgr_disable_auto_stop(1000);
#endif
        /* Inicia el servicio de provisionamiento */
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void *) sec_params, service_name, service_key));

        /* El manejador para el punto final opcional creado anteriormente.
         * Esta llamada debe hacerse después de iniciar el provisionamiento,
         * y solo si el punto final ya se ha creado anteriormente.
         */
        wifi_prov_mgr_endpoint_register("custom-data", custom_prov_data_handler, NULL);

        /* Imprime el código QR para el provisionamiento */
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
        wifi_prov_print_qr(service_name, username, pop, PROV_TRANSPORT_BLE);
#else /* CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP */
        wifi_prov_print_qr(service_name, username, pop, PROV_TRANSPORT_SOFTAP);
#endif
    } else {
        ESP_LOGI(TAG, "Ya aprovisionado, iniciando WiFi en modo estación");

        /* No necesitamos el gestor ya que el dispositivo ya está aprovisionado,
         * así que liberamos sus recursos */
        wifi_prov_mgr_deinit();

        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
        /* Inicia el WiFi en modo estación */
        wifi_init_sta();
    }

    /* Espera a que se conecte el WiFi */
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, true, true, portMAX_DELAY);

    /* Inicia la aplicación principal */
#if CONFIG_EXAMPLE_REPROVISIONING
    while (1) {
        // Cada 10 segundos, imprime un mensaje
        for (int i = 0; i < 10; i++) {
            ESP_LOGI(TAG, "¡Hola Mundo!");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        /* Reinicia la máquina de estados del provisioning para permitir re-aprovisionamiento */
        wifi_prov_mgr_reset_sm_state_for_reprovision();

        /* Espera a que se conecte el WiFi */
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, true, true, portMAX_DELAY);
    }
#else
     while (1) {
         ESP_LOGI(TAG, "¡Hola Mundo!");
         vTaskDelay(1000 / portTICK_PERIOD_MS);
     }
#endif

}

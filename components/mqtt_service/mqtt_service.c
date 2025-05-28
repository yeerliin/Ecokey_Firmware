#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "mqtt_service.h"
#include "wifi_sta.h"
#include "stdarg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_manager.h"
#include "ble_scanner.h"
#include "relay_controller.h" // Añade esta línea para controlar el relé
#include "app_control.h" // Asegúrate de incluir el header adecuado

extern const uint8_t ca_pem_start[] asm("_binary_ca_pem_start");
extern const uint8_t ca_pem_end[]   asm("_binary_ca_pem_end");

static const char *TAG = "mqtt_service";
static esp_mqtt_client_handle_t mqtt_client = NULL;

static TaskHandle_t mqtt_reconnect_task_handle = NULL;
static volatile bool mqtt_reconnect_pending = false;
static uint32_t mqtt_backoff_ms = 1000; // 1 segundo inicial
static const uint32_t mqtt_backoff_max_ms = 300000; // 5 minutos máximo

static char dispositivo_topic[64] = {0}; // Buffer para el tópico dinámico

// Tarea dedicada para reconexión con backoff
static void mqtt_reconnect_task(void *pvParameters)
{
    while (1) {
        if (mqtt_reconnect_pending) {
            ESP_LOGI(TAG, "Backoff antes de reintentar conexión: %" PRIu32 " ms", mqtt_backoff_ms);
            vTaskDelay(mqtt_backoff_ms / portTICK_PERIOD_MS);
            mqtt_backoff_ms *= 2;
            if (mqtt_backoff_ms > mqtt_backoff_max_ms) mqtt_backoff_ms = mqtt_backoff_max_ms;
            // Intentar reconectar
            if (mqtt_client) {
                esp_mqtt_client_stop(mqtt_client);
                esp_mqtt_client_start(mqtt_client);
            }
            mqtt_reconnect_pending = false;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Pequeño retardo para evitar busy loop
    }
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
}

//========================================================================================================================================================================
//========================================================================================================================================================================

// Declaraciones de funciones para cada clave
static void procesar_estado(const char *valor)
{
    ESP_LOGI(TAG, "Procesando estado: %s", valor);
}
static void procesar_luz(const char *valor)
{
    ESP_LOGI(TAG, "Procesando luz: %s", valor);
}
static void procesar_fecha(const char *valor)
{
    ESP_LOGI(TAG, "Procesando fecha: %s", valor);
}
static void procesar_mac(const char *valor)
{
    ESP_LOGI(TAG, "Procesando mac: %s", valor);
}

typedef void (*procesar_func_t)(const char *);
typedef struct
{
    const char *clave;
    procesar_func_t funcion;
} tabla_procesamiento_t;

static tabla_procesamiento_t tabla[] = {
    {"estado", procesar_estado},
    {"luz", procesar_luz},
    {"fecha", procesar_fecha},
    {"mac", procesar_mac},
};
static const int tabla_size = sizeof(tabla) / sizeof(tabla[0]);

//========================================================================================================================================================================
//========================================================================================================================================================================

// Puedes definir aquí la acción a tomar cuando se recibe el estado remoto
static void procesar_estado_remoto(bool estado) {
    ESP_LOGI(TAG, "Procesando Estado remoto: %s (prioridad REMOTA, fuerza modo MANUAL)", estado ? "true" : "false");
    app_control_cambiar_estado(ESTADO_MANUAL);
    nvs_manager_set_int("estado_rele", estado ? 1 : 0);
    // Aplicar el estado al relé
    relay_controller_set_state(estado);
}

// Procesa el mensaje recibido por MQTT según el tópico y el JSON recibido
static void mqtt_service_procesar_mensaje(const char *topic, const char *json)
{
    extern char dispositivo_topic[]; // ya declarado arriba
    if (strcmp(topic, dispositivo_topic) == 0) {
        cJSON *root = cJSON_Parse(json);
        if (root) {
            // Procesar macObjetivo
            cJSON *mac_obj = cJSON_GetObjectItem(root, "macObjetivo");
            if (mac_obj && cJSON_IsString(mac_obj)) {
                ESP_LOGI(TAG, "Actualizando mac_objetivo en NVS: %s", mac_obj->valuestring);
                nvs_manager_set_string("mac_objetivo", mac_obj->valuestring);
                ble_scanner_configurar_mac_objetivo_texto(0, mac_obj->valuestring);
            }
            // Procesar temporizador
            cJSON *temp_obj = cJSON_GetObjectItem(root, "temporizador");
            if (temp_obj && cJSON_IsNumber(temp_obj)) {
                char temp_str[8];
                snprintf(temp_str, sizeof(temp_str), "%d", temp_obj->valueint);
                ESP_LOGI(TAG, "Actualizando temporizador en NVS: %s minutos", temp_str);
                nvs_manager_set_string("temporizador", temp_str);
            }
            // Procesar Estado (booleano)
            cJSON *estado_obj = cJSON_GetObjectItem(root, "Estado");
            if (estado_obj && cJSON_IsBool(estado_obj)) {
                bool estado = cJSON_IsTrue(estado_obj);
                procesar_estado_remoto(estado);
            }
            cJSON_Delete(root);
        }
    } else {
        cJSON *root = cJSON_Parse(json);
        if (root)
        {
            cJSON *item = NULL;
            cJSON_ArrayForEach(item, root)
            {
                if (cJSON_IsString(item))
                {
                    for (int i = 0; i < tabla_size; ++i)
                    {
                        if (strcmp(item->string, tabla[i].clave) == 0)
                        {
                            tabla[i].funcion(item->valuestring);
                            break;
                        }
                    }
                }
            }
            cJSON_Delete(root);
            printf("TOPIC=%s\r\n", topic);
            printf("DATA=%s\r\n", json);
        }
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        // Aquí puedes manejar la lógica después de una conexión exitosa
        // Añadir un funcion para comprobar si es la primera vez que se conecta
        const char *mac_clean = sta_wifi_get_mac_clean(); // MAC sin dos puntos
        snprintf(dispositivo_topic, sizeof(dispositivo_topic), "dispositivos/%s", mac_clean);
        mqtt_service_suscribirse(dispositivo_topic, 1);
        mqtt_backoff_ms = 1000; // Reset backoff al conectar
        break;
    case MQTT_EVENT_DISCONNECTED:
        // Aquí puedes manejar la reconexión o cualquier otra lógica necesaria
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_reconnect_pending = true;
        break;
    case MQTT_EVENT_SUBSCRIBED:
        // Aquí puedes manejar la lógica después de una suscripción exitosa, Por ejemplo, publicar un mensaje en el tópico al que te has suscrito
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "topic/qos0", "Me he suscrito al qos0", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        // Aquí puedes manejar la lógica después de una desuscripción exitosa
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        // Aquí puedes manejar la lógica después de que un mensaje se haya publicado exitosamente
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA: {
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        char json_buffer[256];
        int len = event->data_len < sizeof(json_buffer) - 1 ? event->data_len : sizeof(json_buffer) - 1;
        memcpy(json_buffer, event->data, len);
        json_buffer[len] = '\0';

        char topic_buffer[128];
        int tlen = event->topic_len < sizeof(topic_buffer) - 1 ? event->topic_len : sizeof(topic_buffer) - 1;
        memcpy(topic_buffer, event->topic, tlen);
        topic_buffer[tlen] = '\0';

        mqtt_service_procesar_mensaje(topic_buffer, json_buffer);
        break;
    }
    case MQTT_EVENT_ERROR:
        // Aquí puedes manejar errores de conexión o de transporte
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        mqtt_reconnect_pending = true;
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

//========================================================================================================================================================================
//========================================================================================================================================================================

void mqtt_service_enviar_dato(const char *topic, const char *valor, int qos, int retain)
{
    if (qos < 0 || qos > 2) {
        ESP_LOGE(TAG, "QoS inválido (%d). Debe ser 0, 1 o 2. Usando QoS=1 por defecto.", qos);
        qos = 1;
    }
    if (mqtt_client != NULL)
    {
        int msg_id = esp_mqtt_client_publish(mqtt_client, topic, valor, 0, qos, retain);
        ESP_LOGI(TAG, "Mensaje enviado al topic %s: %s (ID=%d, QoS=%d, retain=%d)", topic, valor, msg_id, qos, retain);
    }
    else
    {
        ESP_LOGE(TAG, "No se puede enviar mensaje: cliente MQTT no inicializado");
    }
}

int mqtt_service_suscribirse(const char *topic, int qos)
{
    if (mqtt_client != NULL)
    {
        int msg_id = esp_mqtt_client_subscribe(mqtt_client, topic, qos);
        ESP_LOGI(TAG, "Suscripción al tópico %s con QoS %d (ID=%d)", topic, qos, msg_id);
        return msg_id;
    }
    else
    {
        ESP_LOGE(TAG, "No se puede suscribir: cliente MQTT no inicializado");
        return -1;
    }
}

void mqtt_service_enviar_json(const char *topic, int qos, int retain, ...)
{
    if (qos < 0 || qos > 2) {
        ESP_LOGE(TAG, "QoS inválido (%d) en JSON. Debe ser 0, 1 o 2. Usando QoS=1 por defecto.", qos);
        qos = 1;
    }
    char json_buffer[256];
    char *ptr = json_buffer;
    int remaining = sizeof(json_buffer);
    va_list args;
    va_start(args, retain);

    int written = snprintf(ptr, remaining, "{");
    ptr += written;
    remaining -= written;

    int first = 1;
    const char *clave;
    while ((clave = va_arg(args, const char *)) != NULL) {
        const char *valor = va_arg(args, const char *);
        if (!valor) break;
        written = snprintf(ptr, remaining, "%s\"%s\":\"%s\"", first ? "" : ",", clave, valor);
        ptr += written;
        remaining -= written;
        first = 0;
    }

    snprintf(ptr, remaining, "}");

    va_end(args);

    mqtt_service_enviar_dato(topic, json_buffer, qos, retain);
}

void mqtt_service_start(void)
{
    if (mqtt_client != NULL)
    {
        ESP_LOGI(TAG, "MQTT service ya iniciado (idempotente)");
        return;
    }
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .broker.verification.certificate = (const char *)ca_pem_start,
        .credentials.username = CONFIG_MQTT_USERNAME,
        .credentials.authentication.password = CONFIG_MQTT_PASSWORD,
    };

#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];
    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0)
    {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128)
        {
            int c = fgetc(stdin);
            if (c == '\n')
            {
                line[count] = '\0';
                break;
            }
            else if (c > 0 && c < 127)
            {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    }
    else
    {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    // Crear la tarea de reconexión si no existe
    if (mqtt_reconnect_task_handle == NULL) {
        xTaskCreate(mqtt_reconnect_task, "mqtt_reconnect_task", 4096, NULL, 5, &mqtt_reconnect_task_handle);
    }
}

void mqtt_service_stop(void)
{
    if (mqtt_client == NULL)
    {
        ESP_LOGI(TAG, "MQTT service ya detenido (idempotente)");
        return;
    }
    if (mqtt_client != NULL)
    {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        ESP_LOGI(TAG, "MQTT service stopped and resources released");
    }
}

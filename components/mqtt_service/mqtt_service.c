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

static const char *TAG = "mqtt_service";
static esp_mqtt_client_handle_t mqtt_client = NULL;

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
        /////////////// OBTENER MAC Y PUBLICAR EN TOPICO ////////////////////
        const char *mac = sta_wifi_get_mac_str();
        {
            mqtt_service_enviar_json("dispositivos/nuevo", 2, 1, "mac", mac, "fecha", "12h", NULL);
        }

        /////////////////////////////////////////////////////////////////

        break;
    case MQTT_EVENT_DISCONNECTED:
        // Aquí puedes manejar la reconexión o cualquier otra lógica necesaria
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
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
    case MQTT_EVENT_DATA:
        // Aquí puedes manejar la lógica cuando se recibe un mensaje en un tópico suscrito
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        char json_buffer[256];
        int len = event->data_len < sizeof(json_buffer) - 1 ? event->data_len : sizeof(json_buffer) - 1;
        memcpy(json_buffer, event->data, len);
        json_buffer[len] = '\0';

        cJSON *root = cJSON_Parse(json_buffer);
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
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
        }
        break;
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

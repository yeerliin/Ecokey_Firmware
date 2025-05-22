#include "mqtt_client.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_event.h"
#include "relay_controller.h"
#include "app_control.h"
#include "nvs_manager.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "MQTT_CLIENT";
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

// Temas MQTT
#define MQTT_TOPIC_RELAY_STATE      "ecokey/relay/state"
#define MQTT_TOPIC_APP_STATE        "ecokey/app/state"
#define MQTT_TOPIC_COMMAND          "ecokey/command"

// Posibles comandos
#define CMD_SET_MANUAL             "set_manual"
#define CMD_SET_AUTO               "set_auto"
#define CMD_TOGGLE_RELAY           "toggle_relay"

// Función de callback para eventos MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT conectado");
            mqtt_connected = true;
            
            // Suscribirse al tema de comandos
            esp_mqtt_client_subscribe(client, MQTT_TOPIC_COMMAND, 1);
            
            // Publicar estados iniciales
            mqtt_client_publish_relay_state();
            mqtt_client_publish_app_state();
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT desconectado");
            mqtt_connected = false;
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT datos recibidos");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            
            // Procesar comandos recibidos
            if (strncmp(event->topic, MQTT_TOPIC_COMMAND, event->topic_len) == 0) {
                if (strncmp(event->data, CMD_SET_MANUAL, event->data_len) == 0) {
                    app_control_lanzar_transicion(ESTADO_MANUAL, "MQTT");
                } 
                else if (strncmp(event->data, CMD_SET_AUTO, event->data_len) == 0) {
                    app_control_lanzar_transicion(ESTADO_AUTOMATICO, "MQTT");
                }
                else if (strncmp(event->data, CMD_TOGGLE_RELAY, event->data_len) == 0) {
                    // Solo alternar relé si estamos en modo manual
                    if (app_control_obtener_estado_actual() == ESTADO_MANUAL) {
                        estado_manual_alternar_rele();
                        mqtt_client_publish_relay_state();
                    } else {
                        ESP_LOGW(TAG, "No se puede alternar relé: no estamos en modo manual");
                    }
                }
            }
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;
            
        default:
            ESP_LOGD(TAG, "Otro evento id:%d", event_id);
            break;
    }
}

esp_err_t mqtt_client_init(void)
{
    if (mqtt_client != NULL) {
        ESP_LOGW(TAG, "Cliente MQTT ya inicializado");
        return ESP_OK;
    }
    
    // Leer configuración MQTT desde NVS
    char mqtt_broker[64] = "mqtt://200.234.232.238:1883";  // Valor por defecto
    char mqtt_user[32] = "yerlin";
    char mqtt_pass[32] = "yerlin1234";
    
    nvs_manager_get_string("mqtt_broker", mqtt_broker, sizeof(mqtt_broker));
    nvs_manager_get_string("mqtt_user", mqtt_user, sizeof(mqtt_user));
    nvs_manager_get_string("mqtt_pass", mqtt_pass, sizeof(mqtt_pass));
    
    ESP_LOGI(TAG, "Iniciando cliente MQTT con broker: %s", mqtt_broker);
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_broker,
    };
    
    // Configurar usuario/contraseña si están disponibles
    if (strlen(mqtt_user) > 0) {
        mqtt_cfg.credentials.username = mqtt_user;
        if (strlen(mqtt_pass) > 0) {
            mqtt_cfg.credentials.authentication.password = mqtt_pass;
        }
    }
    
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Error al inicializar cliente MQTT");
        return ESP_FAIL;
    }
    
    // Registrar manejador de eventos
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    // Iniciar cliente MQTT
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al iniciar cliente MQTT: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Cliente MQTT iniciado correctamente");
    return ESP_OK;
}

esp_err_t mqtt_client_stop(void)
{
    if (mqtt_client == NULL) {
        ESP_LOGW(TAG, "Cliente MQTT no inicializado");
        return ESP_OK;
    }
    
    esp_err_t ret = esp_mqtt_client_stop(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al detener cliente MQTT: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_mqtt_client_destroy(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al destruir cliente MQTT: %s", esp_err_to_name(ret));
        return ret;
    }
    
    mqtt_client = NULL;
    mqtt_connected = false;
    
    ESP_LOGI(TAG, "Cliente MQTT detenido correctamente");
    return ESP_OK;
}

esp_err_t mqtt_client_publish_relay_state(void)
{
    if (mqtt_client == NULL || !mqtt_connected) {
        ESP_LOGW(TAG, "Cliente MQTT no conectado");
        return ESP_FAIL;
    }
    
    // Obtener estado actual del relé
    bool relay_state = false;
    esp_err_t ret = relay_controller_get_state(&relay_state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al obtener estado del relé: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Publicar estado
    const char *state_str = relay_state ? "ON" : "OFF";
    int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_RELAY_STATE, state_str, 0, 1, 0);
    
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Error al publicar estado del relé");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Estado del relé publicado: %s", state_str);
    return ESP_OK;
}

esp_err_t mqtt_client_publish_app_state(void)
{
    if (mqtt_client == NULL || !mqtt_connected) {
        ESP_LOGW(TAG, "Cliente MQTT no conectado");
        return ESP_FAIL;
    }
    
    // Obtener estado actual de la aplicación
    estado_app_t app_state = app_control_obtener_estado_actual();
    const char *state_str;
    
    switch (app_state) {
        case ESTADO_CONFIGURACION:
            state_str = "configuracion";
            break;
        case ESTADO_MANUAL:
            state_str = "manual";
            break;
        case ESTADO_AUTOMATICO:
            state_str = "automatico";
            break;
        default:
            state_str = "desconocido";
            break;
    }
    
    // Publicar estado
    int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_APP_STATE, state_str, 0, 1, 0);
    
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Error al publicar estado de la aplicación");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Estado de la aplicación publicado: %s", state_str);
    return ESP_OK;
}
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
#include "relay_controller.h"
#include "app_control.h"

extern const uint8_t ca_pem_start[] asm("_binary_ca_pem_start");
extern const uint8_t ca_pem_end[]   asm("_binary_ca_pem_end");

static const char *TAG = "mqtt_service";
static esp_mqtt_client_handle_t mqtt_client = NULL;

static TaskHandle_t mqtt_reconnect_task_handle = NULL;
static volatile bool mqtt_reconnect_pending = false;
static uint32_t mqtt_backoff_ms = 1000; // 1 segundo inicial
static const uint32_t mqtt_backoff_max_ms = 300000; // 5 minutos máximo

static char dispositivo_topic[64] = {0}; // Buffer para el tópico dinámico

// Añadimos un buffer para el tópico de OTA, similar al del dispositivo
static char ota_topic[64] = {0};

// Variable global para rastrear el estado de conexión
static bool mqtt_is_connected = false;

// Variable para controlar el procesamiento de mensajes OTA de respuesta
static uint32_t ultimo_mensaje_ota_enviado = 0;

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

// Función para procesar el estado remoto recibido
static void procesar_estado_remoto(bool estado) {
    ESP_LOGI(TAG, "Procesando Estado remoto: %s (prioridad REMOTA, fuerza modo MANUAL)", estado ? "true" : "false");
    app_control_cambiar_estado(ESTADO_MANUAL);
    nvs_manager_set_int("estado_rele", estado ? 1 : 0);
    // Aplicar el estado al relé
    relay_controller_set_state(estado);
}

// Función para procesar el modo remoto recibido
static void procesar_modo_remoto(const char *modo) {
    ESP_LOGI(TAG, "Procesando Modo remoto: %s", modo);
    
    // Obtener el estado actual antes de intentar el cambio
    estado_app_t estado_actual = app_control_obtener_estado_actual();
    
    if (strcasecmp(modo, "manual") == 0) {
        // Implementación más robusta para cambio a modo manual
        if (estado_actual != ESTADO_MANUAL) {
            // Guardar en NVS antes del cambio para persistencia
            nvs_manager_set_string("modo_operacion", "manual");
            ESP_LOGI(TAG, "Cambiando a modo MANUAL por control remoto");
            
            // Pequeña pausa para estabilidad
            vTaskDelay(pdMS_TO_TICKS(100));
            
            // Intentar el cambio de estado
            esp_err_t res = app_control_cambiar_estado(ESTADO_MANUAL);
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "Error al cambiar a modo MANUAL: %s. Reintentando...", esp_err_to_name(res));
                vTaskDelay(pdMS_TO_TICKS(500));
                res = app_control_cambiar_estado(ESTADO_MANUAL);
                
                // Verificar nuevamente
                if (res != ESP_OK) {
                    ESP_LOGE(TAG, "Segundo intento fallido. Forzando reinicio del sistema en 3 segundos...");
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    esp_restart(); // Reinicio del sistema como último recurso
                }
            }
            
            // Verificar que el cambio fue exitoso
            vTaskDelay(pdMS_TO_TICKS(300));
            if (app_control_obtener_estado_actual() == ESTADO_MANUAL) {
                ESP_LOGI(TAG, "Cambio a modo MANUAL confirmado");
            } else {
                ESP_LOGE(TAG, "Estado inconsistente después del cambio a MANUAL");
            }
        } else {
            ESP_LOGI(TAG, "Ya se encuentra en modo MANUAL");
        }
    } 
    else if (strcasecmp(modo, "automatico") == 0) {
        // Implementación similar para modo automático
        if (estado_actual != ESTADO_AUTOMATICO) {
            nvs_manager_set_string("modo_operacion", "automatico");
            ESP_LOGI(TAG, "Cambiando a modo AUTOMÁTICO por control remoto");
            
            vTaskDelay(pdMS_TO_TICKS(100));
            
            esp_err_t res = app_control_cambiar_estado(ESTADO_AUTOMATICO);
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "Error al cambiar a modo AUTOMÁTICO: %s. Reintentando...", esp_err_to_name(res));
                vTaskDelay(pdMS_TO_TICKS(500));
                res = app_control_cambiar_estado(ESTADO_AUTOMATICO);
                
                if (res != ESP_OK) {
                    ESP_LOGE(TAG, "Segundo intento fallido. Forzando reinicio del sistema en 3 segundos...");
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    esp_restart();
                }
            }
            
            vTaskDelay(pdMS_TO_TICKS(300));
            if (app_control_obtener_estado_actual() == ESTADO_AUTOMATICO) {
                ESP_LOGI(TAG, "Cambio a modo AUTOMÁTICO confirmado");
            } else {
                ESP_LOGE(TAG, "Estado inconsistente después del cambio a AUTOMÁTICO");
            }
        } else {
            ESP_LOGI(TAG, "Ya se encuentra en modo AUTOMÁTICO");
        }
    }
    else {
        ESP_LOGW(TAG, "Modo no reconocido: %s (valores válidos: 'manual' o 'automatico')", modo);
    }
}

// Función para procesar mensajes OTA mejorada
static void procesar_mensaje_ota(const char *json) {
    ESP_LOGI(TAG, "Procesando mensaje OTA: %s", json);
    
    // Verificar si es un mensaje de respuesta enviado por nosotros mismos
    if (strstr(json, "\"estado\":") != NULL) {
        uint32_t ahora = esp_log_timestamp();
        // Si recién enviamos un mensaje (menos de 2 segundos), ignorarlo para evitar bucle
        if (ahora - ultimo_mensaje_ota_enviado < 2000) {
            ESP_LOGW(TAG, "Ignorando mensaje OTA que parece ser nuestra propia respuesta");
            return;
        }
    }
    
    // Buscar URL de firmware (primero con método robusto)
    const char *url_str = NULL;
    cJSON *root = cJSON_Parse(json);
    if (root) {
        // Buscar URL de firmware en el JSON
        cJSON *url_obj = cJSON_GetObjectItem(root, "url");
        if (url_obj && cJSON_IsString(url_obj)) {
            url_str = url_obj->valuestring;
            ESP_LOGI(TAG, "URL de actualización recibida: %s", url_str);
            
            // Verificar si la URL es válida
            if (strlen(url_str) < 8) { // Al menos "http://"
                ESP_LOGE(TAG, "URL inválida para OTA: %s", url_str);
                ultimo_mensaje_ota_enviado = esp_log_timestamp(); // Marca temporal
                mqtt_service_enviar_json(ota_topic, 1, 0, "estado", "error", "mensaje", "URL inválida", "tipo", "comando", NULL);
                cJSON_Delete(root);
                return;
            }
            
            // Verificar si hay un campo de versión
            cJSON *version_obj = cJSON_GetObjectItem(root, "version");
            const char *version = version_obj && cJSON_IsString(version_obj) ? 
                                 version_obj->valuestring : "desconocida";
            
            // Verificar si hay un campo de forzar actualización
            bool forzar = false;
            cJSON *force_obj = cJSON_GetObjectItem(root, "force");
            if (force_obj && cJSON_IsBool(force_obj)) {
                forzar = cJSON_IsTrue(force_obj);
            }
            
            ESP_LOGI(TAG, "Iniciando actualización OTA: URL=%s, versión=%s, forzar=%s", 
                    url_str, version, forzar ? "sí" : "no");
                    
            // Aquí iniciarías el proceso OTA con el firmware_url
            // Por ejemplo, llamando a una función de un componente OTA:
            // ota_service_start_update(firmware_url, forzar);
            
            // Reportar que se ha recibido la solicitud de actualización
            ultimo_mensaje_ota_enviado = esp_log_timestamp(); // Marca temporal
            mqtt_service_enviar_json(ota_topic, 1, 0, "estado", "iniciando", "version", version, "tipo", "respuesta", NULL);
            cJSON_Delete(root);
            return;
        }
        
        cJSON_Delete(root);
    }
    
    // Si llegamos aquí, no encontramos una URL válida
    if (!strstr(json, "\"tipo\":\"respuesta\"")) { // No procesar si es una respuesta
        ESP_LOGW(TAG, "Mensaje OTA sin URL de firmware o con formato incorrecto");
        ultimo_mensaje_ota_enviado = esp_log_timestamp(); // Marca temporal
        mqtt_service_enviar_json(ota_topic, 1, 0, "estado", "error", "mensaje", "Falta URL o formato incorrecto", "tipo", "respuesta", NULL);
    }
}

// Procesa el mensaje recibido por MQTT según el tópico y el JSON recibido
static void mqtt_service_procesar_mensaje(const char *topic, const char *json)
{
    if (!topic || !json) {
        ESP_LOGE(TAG, "Topic o mensaje NULL recibido");
        return;
    }
    
    ESP_LOGI(TAG, "Procesando mensaje en tópico: %s", topic);
    
    // Verificar si es un mensaje de respuesta (ignorar para evitar bucles)
    if (strstr(json, "\"tipo\":\"respuesta\"")) {
        ESP_LOGD(TAG, "Ignorando mensaje de respuesta para evitar bucle");
        return;
    }
    
    // Primero verificamos si es un tópico OTA
    if (strcmp(topic, ota_topic) == 0) {
        ESP_LOGI(TAG, "Mensaje OTA recibido en tópico correcto: %s", ota_topic);
        procesar_mensaje_ota(json);
        return; // Procesado como OTA, no continuar
    }
    
    // Si no es OTA, intentamos detectar por contenido
    if (strstr(json, "\"url\":") != NULL && 
        !strstr(json, "\"tipo\":\"respuesta\"")) {
        
        ESP_LOGW(TAG, "Posible mensaje OTA detectado por contenido en tópico incorrecto: %s", topic);
        procesar_mensaje_ota(json);
        return; // Procesado como OTA, no continuar
    }
    
    // Procesamiento de mensajes del dispositivo
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
            
            // Procesar Modo (string)
            cJSON *modo_obj = cJSON_GetObjectItem(root, "Modo");
            if (modo_obj && cJSON_IsString(modo_obj)) {
                procesar_modo_remoto(modo_obj->valuestring);
            }
            
            cJSON_Delete(root);
        } else {
            ESP_LOGE(TAG, "Error al analizar JSON: %s", json);
        }
    } else {
        ESP_LOGW(TAG, "Mensaje recibido en tópico no manejado: %s", topic);
        ESP_LOGW(TAG, "Tópicos registrados: dispositivo=%s, ota=%s", 
                 dispositivo_topic, ota_topic);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    // Eliminadas las variables no utilizadas client y msg_id

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        // Aquí puedes manejar la lógica después de una conexión exitosa
        {
            const char *mac_clean = sta_wifi_get_mac_clean(); // MAC sin dos puntos
            
            // Configuración del tópico de dispositivo (como ya tenías)
            snprintf(dispositivo_topic, sizeof(dispositivo_topic), "dispositivos/%s", mac_clean);
            mqtt_service_suscribirse(dispositivo_topic, 1);
            
            // Configuración del tópico OTA
            snprintf(ota_topic, sizeof(ota_topic), "ota/%s", mac_clean);
            mqtt_service_suscribirse(ota_topic, 1);
            ESP_LOGI(TAG, "Suscrito a tópicos de dispositivo y OTA");
            
            mqtt_backoff_ms = 1000; // Reset backoff al conectar
            mqtt_is_connected = true; // Actualizamos el estado de conexión
        }
        break;
        
    case MQTT_EVENT_DISCONNECTED:
        // Aquí puedes manejar la reconexión o cualquier otra lógica necesaria
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_reconnect_pending = true;
        mqtt_is_connected = false; // Actualizamos el estado de conexión
        break;
        
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_PUBLISHED:
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
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
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

bool mqtt_service_esta_conectado(void) {
    if (mqtt_client == NULL) {
        return false; // Cliente no inicializado
    }
    
    // Usamos nuestra variable de estado en lugar de la función que no está disponible
    return mqtt_is_connected;
}

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

// Modificar mqtt_service_enviar_json para manejar booleanos
void mqtt_service_enviar_json(const char *topic, int qos, int retain, ...)
{
    if (qos < 0 || qos > 2) {
        ESP_LOGE(TAG, "QoS inválido (%d) en JSON. Debe ser 0, 1 o 2. Usando QoS=1 por defecto.", qos);
        qos = 1;
    }
    
    char json_buffer[512]; // Incrementamos el tamaño
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
        mqtt_is_connected = false; // Asegurarse de actualizar el estado
        ESP_LOGI(TAG, "MQTT service stopped and resources released");
    }
}

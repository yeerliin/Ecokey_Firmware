// Inclusión de archivo de cabecera que define la API pública del componente
#include "app_control.h"
// Inclusión para funciones de registro (logging)
#include "esp_log.h"
// Inclusiones para el sistema operativo FreeRTOS
#include "freertos/FreeRTOS.h"
// Inclusión para semáforos de FreeRTOS
#include "freertos/semphr.h"
// Inclusión para acceso a almacenamiento persistente
#include "nvs_manager.h"
// Inclusiones de los módulos de estados específicos
#include "estado_automatico.h"
#include "estado_manual.h"
#include "estado_configuracion.h"
// Inclusión para funciones de manipulación de strings
#include <string.h>
// Inclusión para usar colas de FreeRTOS
#include "freertos/queue.h"
#include "wifi_sta.h"
#include "mqtt_service.h"
#include "time_manager.h"

// === NUEVOS INCLUDES PARA DIAGNÓSTICO DE MEMORIA ===
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/task.h"

// Etiqueta para los mensajes de log de este módulo
static const char *TAG = "APP_CONTROL";

// === CONSTANTES PARA DIAGNÓSTICO DE MEMORIA ===
#define MEMORY_LOW_THRESHOLD_BYTES    (10 * 1024)  // 10KB umbral crítico
#define MEMORY_WARNING_THRESHOLD_BYTES (20 * 1024)  // 20KB umbral advertencia
#define STACK_USAGE_WARNING_BYTES     (1024)       // 1KB advertencia stack

// === FUNCIONES DE DIAGNÓSTICO DE MEMORIA ===

/**
 * @brief Obtiene información detallada del heap
 * 
 * Esta función es segura para llamar desde cualquier contexto y 
 * no modifica el comportamiento existente del sistema.
 */
static void diagnostico_heap_info(void)
{
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    size_t largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    
    ESP_LOGD(TAG, "[MEMORY] Heap libre: %u bytes, mínimo histórico: %u bytes, bloque más grande: %u bytes",
             free_heap, min_free_heap, largest_free_block);
}

/**
 * @brief Verifica si la memoria disponible está en niveles críticos
 * 
 * @return true si la memoria está en nivel crítico, false si es suficiente
 */
static bool diagnostico_memoria_critica(void)
{
    size_t free_heap = esp_get_free_heap_size();
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    
    bool critico = (free_heap < MEMORY_LOW_THRESHOLD_BYTES) || 
                   (largest_block < (MEMORY_LOW_THRESHOLD_BYTES / 2));
    
    if (critico) {
        ESP_LOGW(TAG, "[MEMORY] CRÍTICO: Heap libre: %u bytes, bloque más grande: %u bytes", 
                 free_heap, largest_block);
    }
    
    return critico;
}

/**
 * @brief Registra advertencia si la memoria está en nivel de advertencia
 * 
 * @return true si se registró una advertencia, false si la memoria es suficiente
 */
static bool diagnostico_memoria_advertencia(void)
{
    size_t free_heap = esp_get_free_heap_size();
    
    if (free_heap < MEMORY_WARNING_THRESHOLD_BYTES && free_heap >= MEMORY_LOW_THRESHOLD_BYTES) {
        ESP_LOGW(TAG, "[MEMORY] ADVERTENCIA: Memoria baja: %u bytes libres", free_heap);
        return true;
    }
    
    return false;
}

/**
 * @brief Verifica el uso del stack de la tarea actual
 * 
 * Esta función es útil para detectar posibles desbordamientos de stack
 */
static void diagnostico_stack_usage(void)
{
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(current_task);
    
    // Convertir a bytes (cada word son 4 bytes en ESP32)
    size_t stack_free_bytes = stack_high_water * sizeof(StackType_t);
    
    if (stack_free_bytes < STACK_USAGE_WARNING_BYTES) {
        ESP_LOGW(TAG, "[MEMORY] Stack bajo en tarea actual: %u bytes libres", stack_free_bytes);
    } else {
        ESP_LOGD(TAG, "[MEMORY] Stack OK: %u bytes libres", stack_free_bytes);
    }
}

/**
 * @brief Función integral de diagnóstico que ejecuta todas las verificaciones
 * 
 * Esta función puede ser llamada desde cualquier punto del código para
 * obtener un reporte completo del estado de la memoria.
 */
static void diagnostico_memoria_completo(void)
{
    ESP_LOGD(TAG, "[MEMORY] === DIAGNÓSTICO COMPLETO ===");
    diagnostico_heap_info();
    diagnostico_memoria_critica();
    diagnostico_memoria_advertencia();
    diagnostico_stack_usage();
    ESP_LOGD(TAG, "[MEMORY] === FIN DIAGNÓSTICO ===");
}

// Clave usada para guardar el estado actual en NVS
#define NVS_KEY_ESTADO "app_estado"

// --- Constantes para mejorar legibilidad de logs ---
#define LOG_PREFIX_BOOT "[BOOT]"   // Para logs de inicialización
#define LOG_PREFIX_TRANS "[TRANS]" // Para logs de transición
#define LOG_PREFIX_STATE "[STATE]" // Para logs de estado
#define LOG_PREFIX_NVS "[NVS]"     // Para logs de operaciones NVS

// Estado actual de la aplicación (inicializado como inválido)
static estado_app_t estado_actual = ESTADO_INVALIDO;
// Bandera que indica si el estado actual está inicializado
static bool estado_inicializado = false;

// --- Definición de estructura para tabla de estados ---
// Estructura que contiene la información de cada estado posible
typedef struct
{
    estado_app_t estado;        // Identificador del estado
    esp_err_t (*iniciar)(void); // Puntero a función para iniciar el estado
    esp_err_t (*detener)(void); // Puntero a función para detener el estado
    const char *nombre;         // Nombre legible del estado para logs
} estado_entry_t;

// Tabla que mapea cada estado con sus funciones asociadas y su nombre
static const estado_entry_t tabla_estados[] = {
    {ESTADO_CONFIGURACION, estado_configuracion_iniciar, estado_configuracion_detener, "CONFIGURACION"},
    {ESTADO_MANUAL, estado_manual_iniciar, estado_manual_detener, "MANUAL"},
    {ESTADO_AUTOMATICO, estado_automatico_iniciar, estado_automatico_detener, "AUTOMATICO"},
    // Agrega aquí nuevos estados
};

// Macro que calcula el número de estados en la tabla
#define NUM_ESTADOS (sizeof(tabla_estados) / sizeof(tabla_estados[0]))

/**
 * Busca un estado en la tabla de estados y devuelve su entrada.
 *
 * @param estado El estado a buscar
 * @return Puntero a la entrada del estado o NULL si no se encuentra
 */
static const estado_entry_t *buscar_estado_entry(estado_app_t estado)
{
    // Recorre toda la tabla de estados
    for (size_t i = 0; i < NUM_ESTADOS; ++i)
    {
        // Si encuentra el estado buscado, devuelve un puntero a esa entrada
        if (tabla_estados[i].estado == estado)
            return &tabla_estados[i];
    }
    // Si no encuentra el estado, devuelve NULL
    return NULL;
}

/**
 * Detiene un estado específico llamando a su función de detención.
 *
 * @param estado El estado a detener
 * @return ESP_OK si se detuvo correctamente, o error si falló
 */
static esp_err_t detener_estado(estado_app_t estado)
{
    const estado_entry_t *entry = buscar_estado_entry(estado);
    if (entry && entry->detener)
    {
        // Solo log de error si falla
        esp_err_t res = entry->detener();
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Error al detener estado %s: %s", entry->nombre, esp_err_to_name(res));
        }
        return res;
    }
    return ESP_OK;
}

/**
 * Inicia un estado específico llamando a su función de iniciación.
 *
 * @param estado El estado a iniciar
 * @return ESP_OK si se inició correctamente, o error si falló
 */
static esp_err_t iniciar_estado(estado_app_t estado)
{
    const estado_entry_t *entry = buscar_estado_entry(estado);
    if (entry && entry->iniciar)
    {
        esp_err_t res = entry->iniciar();
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Error al iniciar estado %s: %s", entry->nombre, esp_err_to_name(res));
        }
        return res;
    }
    ESP_LOGE(TAG, "Nuevo estado desconocido: %d", estado);
    return ESP_FAIL;
}

/**
 * Cambia el estado actual del sistema, deteniendo el anterior e iniciando el nuevo.
 * Gestiona WiFi y MQTT según el estado. Guarda el nuevo estado en NVS.
 *
 * @param nuevo_estado El estado al que cambiar
 * @return ESP_OK si el cambio fue exitoso, o error en caso contrario
 */
esp_err_t app_control_cambiar_estado(estado_app_t nuevo_estado)
{
    esp_err_t resultado = ESP_OK;

    // Si el estado solicitado es el mismo que ya tenemos y está inicializado, no hacemos nada
    if (nuevo_estado == estado_actual && estado_inicializado)
    {
        return ESP_OK;
    }

    // Determinamos si es un arranque o una transición normal
    bool es_primer_arranque = (estado_actual == ESTADO_INVALIDO);

    // Log solo en transición crítica
    if (es_primer_arranque)
    {
        ESP_LOGI(TAG, LOG_PREFIX_BOOT " Primer arranque o reinicio → Estado %d", nuevo_estado);
    }

    // Si hay un estado anterior válido y está inicializado, lo detenemos
    if (estado_actual != ESTADO_INVALIDO && estado_inicializado)
    {
        resultado = detener_estado(estado_actual);
        if (resultado != ESP_OK)
        {
            ESP_LOGE(TAG, LOG_PREFIX_STATE " Error al detener %d: %s",
                     estado_actual, esp_err_to_name(resultado));
            return resultado;
        }
    }

    // --- GESTIÓN DE RECURSOS SEGÚN ESTADO ---
    if (nuevo_estado == ESTADO_CONFIGURACION) {
        // Siempre detener estos servicios al entrar en configuración
        mqtt_service_stop();
        sta_wifi_disconnect();
    } else if (nuevo_estado == ESTADO_MANUAL || nuevo_estado == ESTADO_AUTOMATICO) {
        // Inicializar servicios en cualquiera de estos casos:
        // 1. Si venimos de CONFIGURACION (cambio directo)
        // 2. Si es primera inicialización (estado_actual == INVALIDO)
        // 3. Tras un reinicio, verificando si WiFi no está conectado
        if (estado_actual == ESTADO_CONFIGURACION || 
            estado_actual == ESTADO_INVALIDO || 
            !sta_wifi_is_connected()) {
            
            ESP_LOGI(TAG, "Inicializando servicios de conectividad para estado %d", nuevo_estado);
            sta_wifi_init(); // idempotente
            sta_wifi_connect_with_nvs(7000);
            time_manager_init("pool.ntp.org");
            mqtt_service_start(); // idempotente
        }
    }


    // Actualiza la variable de estado actual y registra el cambio
    estado_actual = nuevo_estado;

    // Guarda el nuevo estado en almacenamiento persistente
    app_control_guardar_estado();

    // Inicia el nuevo estado
    resultado = iniciar_estado(nuevo_estado);

    // Maneja el resultado de la iniciación
    if (resultado != ESP_OK)
    {
        ESP_LOGE(TAG, LOG_PREFIX_STATE " Error al iniciar %d: %s",
                 nuevo_estado, esp_err_to_name(resultado));
        estado_inicializado = false;
    }
    else
    {
        estado_inicializado = true;
    }

    return resultado;
}

/**
 * Obtiene el estado actual del sistema.
 *
 * @return El estado actual de la aplicación
 */
estado_app_t app_control_obtener_estado_actual(void)
{
    // Simplemente devuelve la variable de estado actual
    return estado_actual;
}

/**
 * Obtiene el estado actual del sistema de forma segura.
 * Esta implementación siempre es segura para llamar, incluso desde ISRs.
 *
 * @return El estado actual de la aplicación
 */
estado_app_t app_control_get_estado(void)
{
    return estado_actual;
}

/**
 * Guarda el estado actual en almacenamiento persistente (NVS).
 *
 * @return ESP_OK si se guardó correctamente, o error en caso contrario
 */
esp_err_t app_control_guardar_estado(void)
{
    esp_err_t err = nvs_manager_set_u8(NVS_KEY_ESTADO, (uint8_t)estado_actual);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, LOG_PREFIX_NVS " Error al guardar estado %d: %s",
                 estado_actual, esp_err_to_name(err));
    }

    return err;
}

/**
 * Inicializa el sistema de estados, recuperando el último estado guardado o
 * estableciendo el estado inicial si es la primera ejecución.
 *
 * @return ESP_OK si se inició correctamente, o error en caso contrario
 */
esp_err_t app_control_iniciar_estado(void)
{
    esp_err_t ret = ESP_OK;
    uint8_t estado_nvs = nvs_manager_get_u8(NVS_KEY_ESTADO, ESTADO_INVALIDO);
    estado_app_t estado = (estado_app_t)estado_nvs;

    // Verificar si existen credenciales WiFi
    if (!nvs_manager_has_wifi_credentials())
    {
        ret = app_control_cambiar_estado(ESTADO_CONFIGURACION);
    }
    else
    {
        // Si hay credenciales, restaurar el último estado guardado, excepto si es CONFIGURACION
        if (estado == ESTADO_CONFIGURACION)
        {
            estado = ESTADO_AUTOMATICO; // Cambiar a un estado por defecto si es CONFIGURACION
        }
        ret = app_control_cambiar_estado(estado);
    }

    return ret;
}

// --- Variables para el sistema de transiciones ---
// Estructura para pasar información a la tarea de transición
typedef struct
{
    estado_app_t destino; // Estado al que transicionar
    char tag[32];         // Etiqueta para mensajes de log
} transicion_args_t;

// Cola para comunicación entre app_control_lanzar_transicion y la tarea de control
static QueueHandle_t transicion_queue = NULL;
// Manejador de la tarea de control de estado
static TaskHandle_t tarea_control_estado_handle = NULL;
// Semáforo para proteger operaciones críticas de transición
static SemaphoreHandle_t transicion_mutex = NULL;
// Estado actualmente en proceso de transición
static estado_app_t estado_en_transicion = ESTADO_INVALIDO;

/**
 * Tarea permanente que procesa las solicitudes de transición de estado.
 * Esta tarea espera mensajes en la cola y realiza los cambios de estado solicitados.
 *
 * @param param Parámetros de la tarea (no utilizados)
 */
static void tarea_control_estado(void *param)
{
    transicion_args_t args;
    while (1)
    {
        if (xQueueReceive(transicion_queue, &args, portMAX_DELAY) == pdTRUE)
        {
            // Tomar el semáforo para evitar condiciones de carrera
            if (xSemaphoreTake(transicion_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Solo procesar si no es el estado actual o si es una reinicialización del mismo estado
                if (args.destino != estado_actual || !estado_inicializado) {
                    estado_en_transicion = args.destino;
                    xSemaphoreGive(transicion_mutex);
                    
                    ESP_LOGI(TAG, LOG_PREFIX_TRANS " Procesando transición a %d desde %s", 
                             args.destino, args.tag);
                    app_control_cambiar_estado(args.destino);
                    
                    // Marcar que ya no hay transición en curso
                    xSemaphoreTake(transicion_mutex, portMAX_DELAY);
                    estado_en_transicion = ESTADO_INVALIDO;
                    xSemaphoreGive(transicion_mutex);
                } else {
                    ESP_LOGW(TAG, LOG_PREFIX_TRANS " Ignorando transición redundante a %d desde %s", 
                             args.destino, args.tag);
                    xSemaphoreGive(transicion_mutex);
                }
            } else {
                ESP_LOGE(TAG, LOG_PREFIX_TRANS " No se pudo obtener el mutex para procesar transición");
            }
            
            // Vaciar la cola de solicitudes redundantes para el mismo destino
            transicion_args_t siguiente;
            while (xQueuePeek(transicion_queue, &siguiente, 0) == pdTRUE) {
                if (siguiente.destino == args.destino) {
                    // Si es el mismo destino, sacar de la cola y descartar
                    xQueueReceive(transicion_queue, &siguiente, 0);
                    ESP_LOGW(TAG, LOG_PREFIX_TRANS " Descartando transición redundante a %d desde %s", 
                             siguiente.destino, siguiente.tag);
                } else {
                    // Si es diferente destino, dejar en la cola
                    break;
                }
            }
        }
    }
}

/**
 * Lanza una transición de estado de forma asíncrona mediante una cola.
 * Esta función es segura para llamar desde cualquier contexto, ya que
 * el cambio real de estado ocurre en una tarea dedicada.
 *
 * @param destino Estado al que se quiere transicionar
 * @param tag Etiqueta para mensajes de log
 * @return ESP_OK si la solicitud se encoló correctamente, o error en caso contrario
 */
esp_err_t app_control_lanzar_transicion(estado_app_t destino, const char *tag)
{
    // Si la cola no existe aún, inicializa el sistema de transiciones
    if (!transicion_queue)
    {
        transicion_queue = xQueueCreate(8, sizeof(transicion_args_t)); // Aumentamos el tamaño de la cola
        transicion_mutex = xSemaphoreCreateMutex();
        
        if (!transicion_queue || !transicion_mutex)
        {
            ESP_LOGE(TAG, LOG_PREFIX_BOOT " No se pudieron crear recursos para transiciones");
            return ESP_ERR_NO_MEM;
        }
        xTaskCreate(tarea_control_estado, "tarea_control_estado", 4096, NULL, tskIDLE_PRIORITY + 2, &tarea_control_estado_handle);
    }

    // Verificar si ya hay una transición en curso para el mismo destino
    bool transicion_duplicada = false;
    if (xSemaphoreTake(transicion_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        transicion_duplicada = (estado_en_transicion == destino);
        xSemaphoreGive(transicion_mutex);
    }
    
    // Si ya hay una transición al mismo destino, no encolar otra
    if (transicion_duplicada) {
        ESP_LOGW(TAG, LOG_PREFIX_TRANS " Ya hay una transición a %d en curso, ignorando solicitud", destino);
        return ESP_OK; // No es un error, simplemente no es necesario
    }

    transicion_args_t args = {.destino = destino};
    strncpy(args.tag, tag ? tag : "TRANSICION", sizeof(args.tag) - 1);
    args.tag[sizeof(args.tag) - 1] = '\0';

    // Enviar a la cola con un tiempo de espera breve
    if (xQueueSend(transicion_queue, &args, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        ESP_LOGE(TAG, LOG_PREFIX_TRANS " Cola llena, transición a %d descartada", destino);
        return ESP_FAIL;
    }

    return ESP_OK;
}

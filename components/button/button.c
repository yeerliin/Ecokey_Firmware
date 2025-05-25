#include "button.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_check.h"

static const char *ETIQUETA = "BOTON";

// Configuración fija del botón
#define BOTON_GPIO              13  // GPIO fijo para el botón (ajustar según sea necesario)
#define BOTON_NIVEL_ACTIVO      false  // false = activo en bajo (pull-up)

// Tiempos predeterminados
#define TIEMPO_DEBOUNCE_MS      50
#define TIEMPO_CORTO_MS         500
#define TIEMPO_LARGO_MS         3000
#define TIEMPO_MUY_LARGO_MS     7000
#define TIEMPO_RESET_MS         12000

// Intervalo máximo entre dos pulsaciones simples para considerar doble pulsación (en ms)
#define INTERVALO_DOBLE_PULSACION_MS 400

// Tamaños de pila para las tareas
#define STACK_SIZE_TAREA_BOTON  4096
#define STACK_SIZE_EVENTO_BOTON 4096

// Estados del botón
typedef enum {
    ESTADO_INACTIVO,    // El botón está en reposo (no presionado)
    ESTADO_PRESIONADO,  // El botón está siendo presionado
} estado_boton_t;

// Eventos para ISR
typedef enum {
    EVENTO_ISR_PRESION,   // Se detectó que el botón fue presionado
    EVENTO_ISR_LIBERACION // Se detectó que el botón fue liberado
} evento_isr_t;

// Estructura interna para la configuración del botón
typedef struct {
    // Estado
    estado_boton_t estado;
    int64_t tiempo_inicio_presion;
    
    // Banderas de notificación
    bool notificado_largo;
    bool notificado_muy_largo;
    bool notificado_reset;
    
    // Callback
    funcion_callback_boton_t callback;
} config_interna_t;

// Variables estáticas
static config_interna_t boton = {0};
static esp_timer_handle_t temporizador_boton = NULL;
static QueueHandle_t cola_eventos = NULL;
static TaskHandle_t tarea_boton = NULL;
static SemaphoreHandle_t mutex_boton = NULL;
static bool inicializado = false;

// Variables para doble pulsación
static int64_t tiempo_ultima_simple = 0;
static bool esperando_segunda_pulsacion = false;

/**
 * @brief Callback del temporizador para umbrales de tiempo
 * 
 * Esta función es llamada por el temporizador cuando se alcanzan distintos umbrales
 * de tiempo mientras el botón está presionado.
 * 
 * @param[in] arg Argumento pasado al temporizador (no usado)
 */
static void callback_temporizador(void *arg)
{
    // Ignoramos el argumento
    (void)arg;
    
    // Protegemos el acceso a variables compartidas
    if (xSemaphoreTake(mutex_boton, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(ETIQUETA, "No se pudo obtener el mutex en el temporizador");
        return;
    }
    
    // Calculamos tiempo de presión
    int64_t duracion = (esp_timer_get_time() / 1000) - boton.tiempo_inicio_presion;
    
    // Solo procesamos si el botón sigue presionado
    if (boton.estado == ESTADO_PRESIONADO) {
        // Simplificación: Verificamos los umbrales en orden creciente
        // y programamos el siguiente temporizador solo si es necesario
        
        // Verificamos umbral de pulsación larga
        if (duracion >= TIEMPO_LARGO_MS && !boton.notificado_largo) {
            ESP_LOGI(ETIQUETA, "Umbral: Largo (%d ms)", TIEMPO_LARGO_MS);
            boton.notificado_largo = true;
            
            // Programamos el próximo temporizador para muy largo
            esp_timer_start_once(temporizador_boton, 
                (TIEMPO_MUY_LARGO_MS - TIEMPO_LARGO_MS) * 1000);
        }
        // Verificamos umbral de pulsación muy larga
        else if (duracion >= TIEMPO_MUY_LARGO_MS && !boton.notificado_muy_largo) {
            ESP_LOGI(ETIQUETA, "Umbral: Muy largo (%d ms)", TIEMPO_MUY_LARGO_MS);
            boton.notificado_muy_largo = true;
            
            // Programamos el próximo temporizador para reset
            esp_timer_start_once(temporizador_boton, 
                (TIEMPO_RESET_MS - TIEMPO_MUY_LARGO_MS) * 1000);
        }
        // Verificamos umbral de reset
        else if (duracion >= TIEMPO_RESET_MS && !boton.notificado_reset) {
            ESP_LOGI(ETIQUETA, "Umbral: Reset (%d ms)", TIEMPO_RESET_MS);
            boton.notificado_reset = true;
            // No programamos más temporizadores
        }
    }
    
    // Liberamos el mutex
    xSemaphoreGive(mutex_boton);
}

/**
 * @brief Rutina de interrupción (ISR) para el botón
 * 
 * @param[in] arg Argumento pasado a la ISR (no usado)
 */
static void IRAM_ATTR isr_boton(void *arg)
{
    (void)arg;
    
    // Leemos el nivel actual del pin GPIO
    int nivel = gpio_get_level(BOTON_GPIO);
    
    // Determinamos tipo de evento
    evento_isr_t evento = ((nivel == 1 && BOTON_NIVEL_ACTIVO) || 
                          (nivel == 0 && !BOTON_NIVEL_ACTIVO)) ? 
                          EVENTO_ISR_PRESION : EVENTO_ISR_LIBERACION;

    // Enviamos el evento a la cola
    BaseType_t despertar_tarea = pdFALSE;
    xQueueSendFromISR(cola_eventos, &evento, &despertar_tarea);
    
    if (despertar_tarea) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Función para tarea de evento
 * 
 * Ejecuta los eventos de pulsación en una tarea separada para no bloquear.
 * 
 * @param[in] param Parámetro pasado a la tarea (tipo de evento)
 */
static void tarea_evento_boton(void* param) {
    tipo_evento_boton_t evento = *(tipo_evento_boton_t*)param;
    free(param);
    
    // Ejecutamos el callback del usuario si existe
    if (boton.callback) {
        boton.callback(evento);
    }
    
    vTaskDelete(NULL);
}

/**
 * @brief Tarea para procesar eventos del botón
 * 
 * @param[in] arg Argumento pasado a la tarea (no usado)
 */
static void tarea_procesar_eventos(void *arg)
{
    (void)arg;
    evento_isr_t evento;
    
    while (1) {
        // Esperamos evento de la cola
        if (xQueueReceive(cola_eventos, &evento, portMAX_DELAY)) {
            // Tomamos mutex para acceso seguro
            if (xSemaphoreTake(mutex_boton, portMAX_DELAY) != pdTRUE) {
                ESP_LOGE(ETIQUETA, "No se pudo obtener el mutex en la tarea");
                continue;
            }
            
            int64_t tiempo_actual = esp_timer_get_time() / 1000;
            
            switch (evento) {
                case EVENTO_ISR_PRESION:
                    if (boton.estado == ESTADO_INACTIVO) {
                        // Debounce
                        vTaskDelay(pdMS_TO_TICKS(TIEMPO_DEBOUNCE_MS));
                        
                        // Verificamos nuevamente
                        int nivel = gpio_get_level(BOTON_GPIO);
                        bool sigue_presionado = (nivel == 1 && BOTON_NIVEL_ACTIVO) || 
                                              (nivel == 0 && !BOTON_NIVEL_ACTIVO);
                        
                        if (sigue_presionado) {
                            boton.tiempo_inicio_presion = esp_timer_get_time() / 1000;
                            boton.estado = ESTADO_PRESIONADO;
                            
                            // Reiniciamos banderas
                            boton.notificado_largo = false;
                            boton.notificado_muy_largo = false;
                            boton.notificado_reset = false;
                            
                            // Notificamos evento de presión
                            if (boton.callback) {
                                boton.callback(BOTON_PRESIONADO);
                            }
                            
                            // Iniciamos temporizador para pulsación larga
                            esp_timer_start_once(temporizador_boton, TIEMPO_LARGO_MS * 1000);
                        }
                    }
                    break;
                    
                case EVENTO_ISR_LIBERACION:
                    if (boton.estado == ESTADO_PRESIONADO) {
                        // Debounce
                        vTaskDelay(pdMS_TO_TICKS(TIEMPO_DEBOUNCE_MS));
                        
                        // Verificamos nuevamente
                        int nivel = gpio_get_level(BOTON_GPIO);
                        bool sigue_liberado = !((nivel == 1 && BOTON_NIVEL_ACTIVO) || 
                                              (nivel == 0 && !BOTON_NIVEL_ACTIVO));
                        
                        if (sigue_liberado) {
                            // Calculamos duración
                            int64_t duracion = tiempo_actual - boton.tiempo_inicio_presion;
                            
                            // Detenemos temporizador
                            esp_timer_stop(temporizador_boton);
                            
                            // Notificamos liberación
                            if (boton.callback) {
                                boton.callback(BOTON_LIBERADO);
                                
                                // Determinamos tipo de pulsación según duración
                                tipo_evento_boton_t tipo_evento;
                                
                                // Simplificación: Evaluamos en orden de mayor a menor duración
                                if (duracion >= TIEMPO_RESET_MS) {
                                    tipo_evento = BOTON_PULSACION_RESET;
                                    ESP_LOGI(ETIQUETA, "Evento: Reset (%lld ms)", duracion);
                                }
                                else if (duracion >= TIEMPO_MUY_LARGO_MS) {
                                    tipo_evento = BOTON_PULSACION_MUY_LARGA;
                                    ESP_LOGI(ETIQUETA, "Evento: Muy largo (%lld ms)", duracion);
                                }
                                else if (duracion >= TIEMPO_LARGO_MS) {
                                    tipo_evento = BOTON_PULSACION_LARGA;
                                    ESP_LOGI(ETIQUETA, "Evento: Largo (%lld ms)", duracion);
                                }
                                else {
                                    // Detección de doble pulsación rápida
                                    int64_t ahora = esp_timer_get_time() / 1000;
                                    if (esperando_segunda_pulsacion && 
                                        (ahora - tiempo_ultima_simple) <= INTERVALO_DOBLE_PULSACION_MS) {
                                        tipo_evento = BOTON_DOBLE_PULSACION;
                                        ESP_LOGI(ETIQUETA, "Evento: Doble pulsación (%lld ms)", ahora - tiempo_ultima_simple);
                                        esperando_segunda_pulsacion = false;
                                        tiempo_ultima_simple = 0;
                                    } else {
                                        tipo_evento = BOTON_PULSACION_SIMPLE;
                                        ESP_LOGI(ETIQUETA, "Evento: Simple (%lld ms)", duracion);
                                        esperando_segunda_pulsacion = true;
                                        tiempo_ultima_simple = ahora;
                                    }
                                }
                                
                                // Para eventos largos, usamos tarea separada
                                if (tipo_evento != BOTON_PULSACION_SIMPLE && tipo_evento != BOTON_DOBLE_PULSACION) {
                                    tipo_evento_boton_t *evento_copia = malloc(sizeof(tipo_evento_boton_t));
                                    
                                    if (evento_copia) {
                                        *evento_copia = tipo_evento;
                                        
                                        // Creamos tarea con más memoria de pila
                                        TaskHandle_t manejador = NULL;
                                        if (xTaskCreate(
                                                tarea_evento_boton,
                                                "evento_boton",
                                                STACK_SIZE_EVENTO_BOTON,
                                                evento_copia,
                                                5,
                                                &manejador) != pdPASS) {
                                            free(evento_copia);
                                            // Si falla, llamamos directamente
                                            boton.callback(tipo_evento);
                                        }
                                    } else {
                                        // Si no hay memoria, llamamos directamente
                                        boton.callback(tipo_evento);
                                    }
                                } else {
                                    // Pulsaciones simples se procesan directamente
                                    boton.callback(tipo_evento);
                                }
                            }
                            
                            // Volvemos a estado inactivo
                            boton.estado = ESTADO_INACTIVO;
                        }
                    }
                    break;
            }
            
            // Liberamos mutex
            xSemaphoreGive(mutex_boton);
        }
    }
}

esp_err_t iniciar_boton(funcion_callback_boton_t callback)
{
    // Verificación de inicialización previa
    if (inicializado) {
        ESP_LOGW(ETIQUETA, "El botón ya está inicializado");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validación de parámetros
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(BOTON_GPIO), ESP_ERR_INVALID_ARG, 
                       ETIQUETA, "GPIO inválido: %d", BOTON_GPIO);
    
    // Creación de mutex
    mutex_boton = xSemaphoreCreateMutex();
    if (!mutex_boton) {
        ESP_LOGE(ETIQUETA, "No se pudo crear el mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Inicializamos la estructura
    boton.estado = ESTADO_INACTIVO;
    boton.callback = callback;
    
    // Creamos la cola para comunicación entre ISR y tarea
    cola_eventos = xQueueCreate(10, sizeof(evento_isr_t));
    if (!cola_eventos) {
        vSemaphoreDelete(mutex_boton);
        mutex_boton = NULL;
        ESP_LOGE(ETIQUETA, "No se pudo crear la cola de eventos");
        return ESP_ERR_NO_MEM;
    }
    
    // Creamos el temporizador
    esp_timer_create_args_t args_timer = {
        .callback = callback_temporizador,
        .name = "timer_boton"
    };
    
    esp_err_t err = esp_timer_create(&args_timer, &temporizador_boton);
    if (err != ESP_OK) {
        vQueueDelete(cola_eventos);
        vSemaphoreDelete(mutex_boton);
        cola_eventos = NULL;
        mutex_boton = NULL;
        ESP_LOGE(ETIQUETA, "Error creando temporizador: %s", esp_err_to_name(err));
        return err;
    }
    
    // Configuramos el pin GPIO
    gpio_config_t conf_gpio = {
        .pin_bit_mask = (1ULL << BOTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE,
        .pull_up_en = !BOTON_NIVEL_ACTIVO,
        .pull_down_en = BOTON_NIVEL_ACTIVO
    };
    
    err = gpio_config(&conf_gpio);
    if (err != ESP_OK) {
        esp_timer_delete(temporizador_boton);
        vQueueDelete(cola_eventos);
        vSemaphoreDelete(mutex_boton);
        temporizador_boton = NULL;
        cola_eventos = NULL;
        mutex_boton = NULL;
        ESP_LOGE(ETIQUETA, "Error configurando GPIO: %s", esp_err_to_name(err));
        return err;
    }
    
    // Instalamos el servicio ISR
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        esp_timer_delete(temporizador_boton);
        vQueueDelete(cola_eventos);
        vSemaphoreDelete(mutex_boton);
        temporizador_boton = NULL;
        cola_eventos = NULL;
        mutex_boton = NULL;
        ESP_LOGE(ETIQUETA, "Error instalando ISR: %s", esp_err_to_name(err));
        return err;
    }
    
    // Registramos handler ISR
    err = gpio_isr_handler_add(BOTON_GPIO, isr_boton, NULL);
    if (err != ESP_OK) {
        esp_timer_delete(temporizador_boton);
        vQueueDelete(cola_eventos);
        vSemaphoreDelete(mutex_boton);
        temporizador_boton = NULL;
        cola_eventos = NULL;
        mutex_boton = NULL;
        ESP_LOGE(ETIQUETA, "Error agregando handler ISR: %s", esp_err_to_name(err));
        return err;
    }
    
    // Creamos la tarea
    BaseType_t task_created = xTaskCreate(
        tarea_procesar_eventos,
        "tarea_boton",
        STACK_SIZE_TAREA_BOTON,
        NULL,
        10,
        &tarea_boton
    );
    
    if (task_created != pdPASS) {
        gpio_isr_handler_remove(BOTON_GPIO);
        esp_timer_delete(temporizador_boton);
        vQueueDelete(cola_eventos);
        vSemaphoreDelete(mutex_boton);
        temporizador_boton = NULL;
        cola_eventos = NULL;
        mutex_boton = NULL;
        ESP_LOGE(ETIQUETA, "No se pudo crear la tarea");
        return ESP_ERR_NO_MEM;
    }
    
    inicializado = true;
    ESP_LOGI(ETIQUETA, "Botón inicializado en GPIO %d (nivel: %s)",
            BOTON_GPIO, BOTON_NIVEL_ACTIVO ? "alto" : "bajo");
    
    return ESP_OK;
}

void detener_boton(void)
{
    // Verificamos si está inicializado
    if (!inicializado) {
        return;
    }
    
    // Tomamos el mutex para asegurar que no hay operaciones en curso
    if (mutex_boton && xSemaphoreTake(mutex_boton, portMAX_DELAY) == pdTRUE) {
        // Eliminamos el handler de interrupción
        gpio_isr_handler_remove(BOTON_GPIO);
        
        // Detenemos y eliminamos el temporizador
        if (temporizador_boton) {
            esp_timer_stop(temporizador_boton);
            esp_timer_delete(temporizador_boton);
            temporizador_boton = NULL;
        }
        
        // Liberamos mutex antes de eliminar la tarea
        xSemaphoreGive(mutex_boton);
        
        // Eliminamos la tarea
        if (tarea_boton) {
            vTaskDelete(tarea_boton);
            tarea_boton = NULL;
        }
        
        // Eliminamos la cola
        if (cola_eventos) {
            vQueueDelete(cola_eventos);
            cola_eventos = NULL;
        }
        
        // Eliminamos el mutex
        vSemaphoreDelete(mutex_boton);
        mutex_boton = NULL;
    }
    
    // Marcamos como no inicializado
    inicializado = false;
    ESP_LOGI(ETIQUETA, "Botón detenido correctamente");
}

bool boton_esta_presionado(void)
{
    // Verificamos inicialización
    if (!inicializado) {
        return false;
    }
    
    // Leemos el nivel del pin
    int nivel = gpio_get_level(BOTON_GPIO);
    return (nivel == 1 && BOTON_NIVEL_ACTIVO) || (nivel == 0 && !BOTON_NIVEL_ACTIVO);
}

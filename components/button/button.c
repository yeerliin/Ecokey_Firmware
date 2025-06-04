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
    (void)arg;
    if (xSemaphoreTake(mutex_boton, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(ETIQUETA, "No se pudo obtener el mutex en el temporizador");
        return;
    }
    int64_t duracion = (esp_timer_get_time() / 1000) - boton.tiempo_inicio_presion;
    if (boton.estado == ESTADO_PRESIONADO) {
        if (duracion >= TIEMPO_LARGO_MS && !boton.notificado_largo) {
            boton.notificado_largo = true;
            esp_timer_start_once(temporizador_boton, 
                (TIEMPO_MUY_LARGO_MS - TIEMPO_LARGO_MS) * 1000);
        }
        else if (duracion >= TIEMPO_MUY_LARGO_MS && !boton.notificado_muy_largo) {
            boton.notificado_muy_largo = true;
            esp_timer_start_once(temporizador_boton, 
                (TIEMPO_RESET_MS - TIEMPO_MUY_LARGO_MS) * 1000);
        }
        else if (duracion >= TIEMPO_RESET_MS && !boton.notificado_reset) {
            boton.notificado_reset = true;
        }
    }
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
        if (xQueueReceive(cola_eventos, &evento, portMAX_DELAY)) {
            if (xSemaphoreTake(mutex_boton, portMAX_DELAY) != pdTRUE) {
                ESP_LOGE(ETIQUETA, "No se pudo obtener el mutex en la tarea");
                continue;
            }
            int64_t tiempo_actual = esp_timer_get_time() / 1000;
            switch (evento) {
                case EVENTO_ISR_PRESION:
                    if (boton.estado == ESTADO_INACTIVO) {
                        vTaskDelay(pdMS_TO_TICKS(TIEMPO_DEBOUNCE_MS));
                        int nivel = gpio_get_level(BOTON_GPIO);
                        bool sigue_presionado = (nivel == 1 && BOTON_NIVEL_ACTIVO) || 
                                              (nivel == 0 && !BOTON_NIVEL_ACTIVO);
                        if (sigue_presionado) {
                            boton.tiempo_inicio_presion = esp_timer_get_time() / 1000;
                            boton.estado = ESTADO_PRESIONADO;
                            boton.notificado_largo = false;
                            boton.notificado_muy_largo = false;
                            boton.notificado_reset = false;
                            if (boton.callback) {
                                boton.callback(BOTON_PRESIONADO);
                            }
                            esp_timer_start_once(temporizador_boton, TIEMPO_LARGO_MS * 1000);
                        }
                    }
                    break;
                case EVENTO_ISR_LIBERACION:
                    if (boton.estado == ESTADO_PRESIONADO) {
                        vTaskDelay(pdMS_TO_TICKS(TIEMPO_DEBOUNCE_MS));
                        int nivel = gpio_get_level(BOTON_GPIO);
                        bool sigue_liberado = !((nivel == 1 && BOTON_NIVEL_ACTIVO) || 
                                              (nivel == 0 && !BOTON_NIVEL_ACTIVO));
                        if (sigue_liberado) {
                            int64_t duracion = tiempo_actual - boton.tiempo_inicio_presion;
                            esp_timer_stop(temporizador_boton);
                            if (boton.callback) {
                                boton.callback(BOTON_LIBERADO);
                                tipo_evento_boton_t tipo_evento;
                                if (duracion >= TIEMPO_RESET_MS) {
                                    tipo_evento = BOTON_PULSACION_RESET;
                                }
                                else if (duracion >= TIEMPO_MUY_LARGO_MS) {
                                    tipo_evento = BOTON_PULSACION_MUY_LARGA;
                                }
                                else if (duracion >= TIEMPO_LARGO_MS) {
                                    tipo_evento = BOTON_PULSACION_LARGA;
                                }
                                else {
                                    int64_t ahora = esp_timer_get_time() / 1000;
                                    if (esperando_segunda_pulsacion && 
                                        (ahora - tiempo_ultima_simple) <= INTERVALO_DOBLE_PULSACION_MS) {
                                        tipo_evento = BOTON_DOBLE_PULSACION;
                                        esperando_segunda_pulsacion = false;
                                        tiempo_ultima_simple = 0;
                                    } else {
                                        tipo_evento = BOTON_PULSACION_SIMPLE;
                                        esperando_segunda_pulsacion = true;
                                        tiempo_ultima_simple = ahora;
                                    }
                                }
                                if (tipo_evento != BOTON_PULSACION_SIMPLE && tipo_evento != BOTON_DOBLE_PULSACION) {
                                    tipo_evento_boton_t *evento_copia = malloc(sizeof(tipo_evento_boton_t));
                                    if (evento_copia) {
                                        *evento_copia = tipo_evento;
                                        TaskHandle_t manejador = NULL;
                                        if (xTaskCreate(
                                                tarea_evento_boton,
                                                "evento_boton",
                                                STACK_SIZE_EVENTO_BOTON,
                                                evento_copia,
                                                5,
                                                &manejador) != pdPASS) {
                                            free(evento_copia);
                                            boton.callback(tipo_evento);
                                        }
                                    } else {
                                        boton.callback(tipo_evento);
                                    }
                                } else {
                                    boton.callback(tipo_evento);
                                }
                            }
                            boton.estado = ESTADO_INACTIVO;
                        }
                    }
                    break;
            }
            xSemaphoreGive(mutex_boton);
        }
    }
}

esp_err_t iniciar_boton(funcion_callback_boton_t callback)
{
    if (inicializado) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(BOTON_GPIO), ESP_ERR_INVALID_ARG, 
                       ETIQUETA, "GPIO inválido: %d", BOTON_GPIO);
    mutex_boton = xSemaphoreCreateMutex();
    if (!mutex_boton) {
        ESP_LOGE(ETIQUETA, "No se pudo crear el mutex");
        return ESP_ERR_NO_MEM;
    }
    boton.estado = ESTADO_INACTIVO;
    boton.callback = callback;
    cola_eventos = xQueueCreate(10, sizeof(evento_isr_t));
    if (!cola_eventos) {
        vSemaphoreDelete(mutex_boton);
        mutex_boton = NULL;
        ESP_LOGE(ETIQUETA, "No se pudo crear la cola de eventos");
        return ESP_ERR_NO_MEM;
    }
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
    if (!inicializado) {
        return;
    }
    if (mutex_boton && xSemaphoreTake(mutex_boton, portMAX_DELAY) == pdTRUE) {
        gpio_isr_handler_remove(BOTON_GPIO);
        if (temporizador_boton) {
            esp_timer_stop(temporizador_boton);
            esp_timer_delete(temporizador_boton);
            temporizador_boton = NULL;
        }
        xSemaphoreGive(mutex_boton);
        if (tarea_boton) {
            vTaskDelete(tarea_boton);
            tarea_boton = NULL;
        }
        if (cola_eventos) {
            vQueueDelete(cola_eventos);
            cola_eventos = NULL;
        }
        vSemaphoreDelete(mutex_boton);
        mutex_boton = NULL;
    }
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

#include <stdio.h>
#include "led.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_check.h"

// Definición interna del GPIO y polaridad del LED
#define LED_GPIO_NUM GPIO_NUM_4
#define LED_ACTIVE_HIGH true

// Variables globales optimizadas
static const char *TAG = "LED";
#define LED_TASK_STACK_SIZE  2048  // blinking task small footprint
#define LED_TASK_PRIORITY    3

// Configuración del LED
typedef struct {
    bool is_initialized;        // Flag de inicialización
    bool current_state;         // Estado actual del LED
    SemaphoreHandle_t mutex;    // Mutex para protección
    
    // Estado del parpadeo
    bool blink_active;          // Si el parpadeo está activo
    led_blink_params_t blink_params; // Parámetros de parpadeo
    TimerHandle_t blink_timer;  // Temporizador para parpadeo simple
    TaskHandle_t blink_task_handle; // Tarea para patrones complejos
} led_ctx_t;

// Contexto global del LED
static led_ctx_t led_ctx = {0};

// Prototipos de funciones privadas
static void led_blink_timer_cb(TimerHandle_t xTimer);
static void led_blink_task(void *pvParameters);
static void led_set_raw(bool on);
static bool led_validate_state(void);

esp_err_t led_init(void)
{
    // Si ya está inicializado, deinicializar primero
    if (led_ctx.is_initialized) {
        led_deinit();
    }
    
    // Inicializar valores
    led_ctx.blink_active = false;
    led_ctx.blink_timer = NULL;
    led_ctx.blink_task_handle = NULL;
    led_ctx.current_state = false;
    
    // Configurar el GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    ESP_RETURN_ON_ERROR(ret, TAG, "Error al configurar GPIO del LED: %d", ret);
    
    // Crear mutex para protección de acceso al LED
    led_ctx.mutex = xSemaphoreCreateMutex();
    if (led_ctx.mutex == NULL) {
        ESP_LOGE(TAG, "Error al crear mutex");
        return ESP_FAIL;
    }
    
    // Apagar el LED inicialmente
    led_set_raw(false);
    
    led_ctx.is_initialized = true;
    ESP_LOGI(TAG, "LED inicializado en GPIO: %d, Activo %s", 
              LED_GPIO_NUM, LED_ACTIVE_HIGH ? "ALTO" : "BAJO");
    
    return ESP_OK;
}

esp_err_t led_set(bool on)
{
    // Validar estado
    ESP_RETURN_ON_FALSE(led_validate_state(), ESP_ERR_INVALID_STATE, TAG, "LED no inicializado");
    
    // Detener parpadeo si está activo
    if (led_ctx.blink_active) {
        led_blink_stop();
        // Pequeño retraso para asegurar que el parpadeo se detiene
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Tomar mutex para acceso seguro al LED
    if (xSemaphoreTake(led_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        led_set_raw(on);
        led_ctx.current_state = on;
        xSemaphoreGive(led_ctx.mutex);
        ESP_LOGI(TAG, "LED %s", on ? "ENCENDIDO" : "APAGADO");
        return ESP_OK;
    }
    
    ESP_LOGW(TAG, "Timeout al intentar acceder al mutex del LED");
    return ESP_ERR_TIMEOUT;
}

esp_err_t led_toggle(void)
{
    // Validar estado
    ESP_RETURN_ON_FALSE(led_validate_state(), ESP_ERR_INVALID_STATE, TAG, "LED no inicializado");
    
    // Si hay parpadeo activo, detenerlo
    if (led_ctx.blink_active) {
        led_blink_stop();
    }
    
    // Tomar mutex para acceso seguro al LED
    if (xSemaphoreTake(led_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        bool new_state = !led_ctx.current_state;
        led_set_raw(new_state);
        led_ctx.current_state = new_state;
        xSemaphoreGive(led_ctx.mutex);
        ESP_LOGI(TAG, "LED alternado a: %s", new_state ? "ENCENDIDO" : "APAGADO");
        return ESP_OK;
    }
    
    ESP_LOGW(TAG, "Timeout al intentar acceder al mutex del LED");
    return ESP_ERR_TIMEOUT;
}

bool led_get_state(void)
{
    if (!led_validate_state()) {
        return false;
    }
    
    bool state = false;
    if (xSemaphoreTake(led_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = led_ctx.current_state;
        xSemaphoreGive(led_ctx.mutex);
    }
    
    return state;
}

static void led_set_raw(bool on)
{
    // Ajustar según polaridad
    bool level = LED_ACTIVE_HIGH ? on : !on;
    gpio_set_level(LED_GPIO_NUM, level ? 1 : 0);
}

static void led_blink_timer_cb(TimerHandle_t xTimer)
{
    static bool led_state = false;
    
    // Verificar si se debe seguir parpadeando
    if (!led_ctx.blink_active) {
        xTimerStop(xTimer, 0);
        led_set_raw(false);
        led_ctx.current_state = false;
        return;
    }
    
    // Alternar estado del LED
    if (xSemaphoreTake(led_ctx.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        led_state = !led_state;
        led_set_raw(led_state);
        led_ctx.current_state = led_state;
        xSemaphoreGive(led_ctx.mutex);
    }
}

static void led_blink_task(void *pvParameters)
{
    // Implementación para patrones complejos de parpadeo
    led_blink_params_t *params = (led_blink_params_t *)pvParameters;
    bool led_state = false;
    uint32_t repeat_counter = 0;

    ESP_LOGI(TAG, "led_blink_task watermark=%u",
             uxTaskGetStackHighWaterMark(NULL));
    
    ESP_LOGI(TAG, "Tarea de parpadeo avanzado iniciada");
    
    while (led_ctx.blink_active) {
        // Ejecutar patrón según el tipo
        switch (params->pattern) {
            case LED_BLINK_SOS:
                // Patrón SOS: 3 cortos, 3 largos, 3 cortos
                // ... (código para implementar SOS)
                break;
                
            case LED_BLINK_CUSTOM:
            case LED_BLINK_NORMAL:
            default:
                // Parpadeo normal o personalizado
                if (xSemaphoreTake(led_ctx.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    led_state = !led_state;
                    led_set_raw(led_state);
                    led_ctx.current_state = led_state;
                    xSemaphoreGive(led_ctx.mutex);
                }
                
                vTaskDelay(pdMS_TO_TICKS(led_state ? params->on_time_ms : params->off_time_ms));
                break;
        }
        
        // Verificar si se alcanzó el límite de repeticiones
        if (params->repeat_count > 0) {
            repeat_counter++;
            if (repeat_counter >= params->repeat_count) {
                break;
            }
        }
    }
    
    // Asegurarse de que el LED quede apagado al terminar
    if (xSemaphoreTake(led_ctx.mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        led_set_raw(false);
        led_ctx.current_state = false;
        xSemaphoreGive(led_ctx.mutex);
    }
    
    ESP_LOGI(TAG, "Tarea de parpadeo avanzado finalizada");
    led_ctx.blink_active = false;
    led_ctx.blink_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t led_blink_start(uint32_t interval_ms)
{
    // Validar estado
    ESP_RETURN_ON_FALSE(led_validate_state(), ESP_ERR_INVALID_STATE, TAG, "LED no inicializado");
    ESP_RETURN_ON_FALSE(interval_ms > 0, ESP_ERR_INVALID_ARG, TAG, "Intervalo debe ser mayor que 0");
    
    // Detener parpadeo activo si existe
    if (led_ctx.blink_active) {
        led_blink_stop();
    }
    
    // Crear temporizador para parpadeo si no existe
    if (led_ctx.blink_timer == NULL) {
        led_ctx.blink_timer = xTimerCreate(
            "led_blink_timer",
            pdMS_TO_TICKS(interval_ms),
            pdTRUE,  // Auto-reload
            0,
            led_blink_timer_cb
        );
        
        if (led_ctx.blink_timer == NULL) {
            ESP_LOGE(TAG, "Error al crear temporizador de parpadeo");
            return ESP_ERR_NO_MEM;
        }
    } else {
        // Actualizar periodo del temporizador
        xTimerChangePeriod(led_ctx.blink_timer, pdMS_TO_TICKS(interval_ms), pdMS_TO_TICKS(100));
    }
    
    // Configurar parámetros de parpadeo simple
    led_ctx.blink_params.on_time_ms = interval_ms / 2;
    led_ctx.blink_params.off_time_ms = interval_ms / 2;
    led_ctx.blink_params.repeat_count = 0; // Infinito
    led_ctx.blink_params.pattern = LED_BLINK_NORMAL;
    
    // Iniciar parpadeo
    led_ctx.blink_active = true;
    if (xTimerStart(led_ctx.blink_timer, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Error al iniciar temporizador de parpadeo");
        led_ctx.blink_active = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Parpadeo iniciado con intervalo de %lu ms", interval_ms);
    return ESP_OK;
}

esp_err_t led_blink_advanced(const led_blink_params_t* params)
{
    // Validar estado y parámetros
    ESP_RETURN_ON_FALSE(led_validate_state(), ESP_ERR_INVALID_STATE, TAG, "LED no inicializado");
    ESP_RETURN_ON_FALSE(params != NULL, ESP_ERR_INVALID_ARG, TAG, "Parámetros inválidos");
    ESP_RETURN_ON_FALSE(params->on_time_ms > 0, ESP_ERR_INVALID_ARG, TAG, "Tiempo de encendido debe ser mayor que 0");
    ESP_RETURN_ON_FALSE(params->off_time_ms > 0, ESP_ERR_INVALID_ARG, TAG, "Tiempo de apagado debe ser mayor que 0");
    
    // Detener parpadeo activo si existe
    if (led_ctx.blink_active) {
        led_blink_stop();
        // Pequeño retraso para asegurar que el parpadeo anterior se detiene
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Para patrones simples, usar temporizador
    if (params->pattern == LED_BLINK_NORMAL && params->on_time_ms == params->off_time_ms) {
        return led_blink_start(params->on_time_ms * 2);
    }
    
    // Para patrones complejos, usar tarea
    led_ctx.blink_params = *params;
    led_ctx.blink_active = true;

    // blinking task uses small buffers; 2048-word stack is sufficient
    BaseType_t ret = xTaskCreate(
        led_blink_task,
        "led_blink_adv",
        LED_TASK_STACK_SIZE,
        &led_ctx.blink_params,
        LED_TASK_PRIORITY,
        &led_ctx.blink_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Error al crear tarea de parpadeo avanzado");
        led_ctx.blink_active = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Parpadeo avanzado iniciado");
    return ESP_OK;
}

esp_err_t led_blink_stop(void)
{
    // Validar estado
    ESP_RETURN_ON_FALSE(led_validate_state(), ESP_ERR_INVALID_STATE, TAG, "LED no inicializado");
    
    // Si no está activo, no hay nada que hacer
    if (!led_ctx.blink_active) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deteniendo parpadeo...");
    
    // Señalizar a la tarea o temporizador que debe terminar
    led_ctx.blink_active = false;
    
    // Detener temporizador si está activo
    if (led_ctx.blink_timer != NULL) {
        xTimerStop(led_ctx.blink_timer, pdMS_TO_TICKS(100));
    }
    
    // Esperar a que termine la tarea (con timeout)
    if (led_ctx.blink_task_handle != NULL) {
        int timeout_count = 0;
        const int max_timeout = 10;
        
        while (led_ctx.blink_task_handle != NULL && timeout_count < max_timeout) {
            vTaskDelay(pdMS_TO_TICKS(10));
            timeout_count++;
        }
        
        if (led_ctx.blink_task_handle != NULL) {
            ESP_LOGW(TAG, "La tarea de parpadeo no finalizó correctamente, forzando terminación");
            vTaskDelete(led_ctx.blink_task_handle);
            led_ctx.blink_task_handle = NULL;
        }
    }
    
    // Asegurar que el LED quede apagado
    if (xSemaphoreTake(led_ctx.mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        led_set_raw(false);
        led_ctx.current_state = false;
        xSemaphoreGive(led_ctx.mutex);
    }
    
    ESP_LOGI(TAG, "Parpadeo detenido");
    return ESP_OK;
}

esp_err_t led_deinit(void)
{
    // Si no está inicializado, no hay nada que hacer
    if (!led_ctx.is_initialized) {
        return ESP_OK;
    }
    
    // Detener parpadeo si está activo
    if (led_ctx.blink_active) {
        led_blink_stop();
    }
    
    // Eliminar temporizador si existe
    if (led_ctx.blink_timer != NULL) {
        xTimerDelete(led_ctx.blink_timer, pdMS_TO_TICKS(100));
        led_ctx.blink_timer = NULL;
    }
    
    // Eliminar mutex
    if (led_ctx.mutex != NULL) {
        vSemaphoreDelete(led_ctx.mutex);
        led_ctx.mutex = NULL;
    }
    
    // Apagar LED antes de deinicializar
    led_set_raw(false);
    
    led_ctx.is_initialized = false;
    ESP_LOGI(TAG, "LED deinicializado");
    
    return ESP_OK;
}

static bool led_validate_state(void)
{
    return led_ctx.is_initialized;
}

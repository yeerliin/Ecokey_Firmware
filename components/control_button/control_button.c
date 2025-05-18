// control_button.c - Componente que maneja la lógica de pulsaciones del botón

#include "control_button.h"
#include "button.h"
#include "app_control.h"
#include "esp_log.h"
#include "relay_controller.h"
#include "nvs_manager.h"

static const char *TAG = "CONTROL_BUTTON";
static estado_app_t estado_anterior = ESTADO_INVALIDO;

// Tipo de función para acciones de pulsaciones
typedef void (*accion_boton_t)(void);

typedef struct {
    tipo_evento_boton_t tipo_evento;
    accion_boton_t accion;
} entrada_accion_boton_t;

// --- HANDLERS ---

static void accion_rapida(void)
{
    if (app_control_obtener_estado_actual() == ESTADO_MANUAL) {
        bool estado;
        relay_controller_get_state(&estado);
        relay_controller_set_state(!estado);
        ESP_LOGI(TAG, "Pulsación rápida: relé %s", !estado ? "ACTIVADO" : "DESACTIVADO");
    }
}

static void accion_larga(void)
{
    estado_app_t actual = app_control_obtener_estado_actual();

    if (actual != ESTADO_CONFIGURACION) {
        estado_anterior = actual;
        ESP_LOGI(TAG, "Larga: cambiando a CONFIGURACION desde %d", actual);
        app_control_lanzar_transicion(ESTADO_CONFIGURACION, TAG);
    } else if (estado_anterior != ESTADO_INVALIDO) {
        ESP_LOGI(TAG, "Larga: volviendo a estado %d", estado_anterior);
        app_control_lanzar_transicion(estado_anterior, TAG);
    } else {
        ESP_LOGW(TAG, "Por defecto Automatico");
        app_control_lanzar_transicion(ESTADO_AUTOMATICO, TAG);
    }
}

static void accion_muy_larga(void)
{
    ESP_LOGI(TAG, "Muy larga: acción alternativa opcional ejecutada");
    // Ejemplo: app_control_lanzar_transicion(ESTADO_DIAGNOSTICO, TAG);
}

static void accion_reset(void)
{
    ESP_LOGI(TAG, "Reset: Iniciando formateo de fábrica...");
    
 
    // Borrar todas las claves NVS
    esp_err_t err = nvs_manager_erase_all();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al borrar NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Borrado de NVS exitoso");
    }
    
    // Pequeña pausa para permitir que se complete el log y el parpadeo
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Reiniciar a valores de fábrica
    ESP_LOGI(TAG, "Reiniciando dispositivo...");
    
    // Asegurar que el estado inicial sea CONFIGURACION al reiniciar
    // Esto es redundante porque al borrar NVS ya no hay estado guardado
    // pero añade protección extra
    nvs_manager_set_u8("app_estado", ESTADO_CONFIGURACION);
    
    // Reiniciar el dispositivo
    esp_restart();
}


// --- TABLA DE ACCIONES ---

static const entrada_accion_boton_t acciones_boton[] = {
    { BOTON_PULSACION_SIMPLE,      accion_rapida },
    { BOTON_PULSACION_LARGA,       accion_larga },
    { BOTON_PULSACION_MUY_LARGA,   accion_muy_larga },
    { BOTON_PULSACION_RESET,       accion_reset },
};

// --- DESPACHADOR ---

static void callback_boton(tipo_evento_boton_t evento)
{
    for (size_t i = 0; i < sizeof(acciones_boton)/sizeof(acciones_boton[0]); ++i) {
        if (acciones_boton[i].tipo_evento == evento) {
            acciones_boton[i].accion();
            return;
        }
    }

    ESP_LOGI(TAG, "Evento no manejado: %d", evento);
}

// --- INICIALIZACIÓN ---

esp_err_t control_button_iniciar(void)
{
    ESP_LOGI(TAG, "Inicializando lógica de botón");
    return iniciar_boton(callback_boton);
}

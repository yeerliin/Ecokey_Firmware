#include "ble_scanner.h"        // Cabecera propia: funciones y definiciones BLE scanner
#include "esp_log.h"            // Funciones de logging de ESP-IDF (ESP_LOGI, ESP_LOGE, etc)
#include "esp_err.h"            // Funciones para manejo de errores ESP-IDF
#include "nvs_flash.h"          // Acceso a NVS (memoria flash para almacenar datos)

#include "nimble/nimble_port.h"             // Motor BLE NimBLE
#include "nimble/nimble_port_freertos.h"     // Integración de NimBLE con FreeRTOS
#include "host/ble_hs.h"                     // Host stack NimBLE
#include "host/ble_gap.h"                    // API de GAP (escaneo, conexión, etc)
#include "esp_nimble_hci.h"                  // Inicialización del HCI (controlador Bluetooth)
#include <ctype.h>                           // Funciones de caracteres (no la usas aún, pero puede ser útil)

static const char *TAG = "BLE_SCANNER";       // TAG para los mensajes de log
static uint8_t ecokey_tag_mac[6] = {0};        // Dirección MAC objetivo que queremos detectar
static bool tag_detected = false;              // Flag que indica si se detectó el tag

/**
 * Callback de escaneo BLE: se llama cada vez que se detecta un dispositivo.
 */
static int ble_app_scan_cb(struct ble_gap_event *event, void *arg)
{
    if (event->type == BLE_GAP_EVENT_DISC) {   // Si el evento es de tipo "dispositivo descubierto"
        const uint8_t *adv_mac = event->disc.addr.val;  // Apuntador a la MAC del dispositivo detectado

        // Muestra la MAC detectada en el log
        ESP_LOGD(TAG, "MAC detectada: %02X:%02X:%02X:%02X:%02X:%02X",
                 adv_mac[5], adv_mac[4], adv_mac[3],
                 adv_mac[2], adv_mac[1], adv_mac[0]);

        // Compara la MAC detectada contra la MAC objetivo (comparación invertida)
        bool match = true;
        for (int i = 0; i < 6; i++) {
            if (adv_mac[i] != ecokey_tag_mac[5 - i]) {
                match = false;
                break;
            }
        }

        if (match) {  // Si coincide
            ESP_LOGD(TAG, "✅ Tag detectado! %02X:%02X:%02X:%02X:%02X:%02X", 
                adv_mac[5], adv_mac[4], adv_mac[3],
                adv_mac[2], adv_mac[1], adv_mac[0]);
            tag_detected = true;
        }
    }
    return 0; // El callback siempre debe devolver 0
}

/**
 * Devuelve si el tag fue detectado y resetea el flag para futuras lecturas.
 */
bool ble_scanner_tag_detected(void)
{
    bool detected = tag_detected;  // Guarda el valor actual
    tag_detected = false;          // Resetea el flag
    return detected;               // Devuelve el estado
}

/**
 * Establece manualmente la dirección MAC que quieres buscar.
 */
void ble_scanner_set_target_mac(const uint8_t *mac)
{
    memcpy(ecokey_tag_mac, mac, 6); // Copia los 6 bytes
    ESP_LOGI(TAG, "MAC objetivo configurada: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
}

/**
 * Configura y lanza escaneo BLE pasivo (sin conectar) de duración infinita.
 */
static void ble_app_scan(void)
{
    struct ble_gap_disc_params scan_params = {0}; // Parámetros por defecto
    scan_params.passive = 1;           // Modo pasivo: solo escucha, no envía paquetes
    scan_params.itvl = 0x0010;          // Intervalo de escaneo
    scan_params.window = 0x0010;        // Tiempo activo de escucha dentro de cada intervalo
    scan_params.filter_duplicates = 0; // No filtra dispositivos duplicados (opcional)

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER,
                          &scan_params, ble_app_scan_cb, NULL); // Lanza escaneo con callback

    if (rc != 0) {
        ESP_LOGE(TAG, "Fallo al iniciar escaneo: %d", rc); // Error si falla
    }
}

/**
 * Ejecuta el stack BLE NimBLE en FreeRTOS.
 */
static void ble_host_task(void *param)
{
    nimble_port_run();              // Corre el stack BLE
    nimble_port_freertos_deinit();   // Libera recursos al salir
}


/**
 * Inicializa el stack BLE y lanza el escaneo.
 */
void ble_scanner_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());        // Inicializa el NVS (obligatorio para BLE)
    ESP_ERROR_CHECK(nimble_port_init());       // Inicializa el stack NimBLE
    ble_hs_cfg.sync_cb = ble_app_scan;         // Define qué hacer cuando se sincroniza el host (arranca escaneo)
    nimble_port_freertos_init(ble_host_task);  // Crea la tarea BLE
    ESP_LOGI(TAG, "BLE scanner inicializado."); 
}


/**
 * Recibe la MAC en formato string ("001B660F4AE2" o "00:1B:66:0F:4A:E2") y la guarda.
 */
bool ble_scanner_set_target_mac_str(const char *mac_str)
{
    uint8_t mac[6] = {0};  // Buffer temporal para parseo

    if (strlen(mac_str) == 12) {
        // Formato plano sin separadores
        for (int i = 0; i < 6; i++) {
            char byte_str[3] = {mac_str[i * 2], mac_str[i * 2 + 1], 0};
            mac[i] = (uint8_t)strtol(byte_str, NULL, 16);
        }
    } else if (strlen(mac_str) == 17) {
        // Formato con separadores (':')
        for (int i = 0; i < 6; i++) {
            char byte_str[3] = {mac_str[i * 3], mac_str[i * 3 + 1], 0};
            mac[i] = (uint8_t)strtol(byte_str, NULL, 16);
        }
    } else {
        ESP_LOGE(TAG, "Formato de MAC inválido: %s", mac_str);
        return false; // Error de formato
    }

    ble_scanner_set_target_mac(mac); // Ahora ya la tienes cargada en binario
    return true;
}

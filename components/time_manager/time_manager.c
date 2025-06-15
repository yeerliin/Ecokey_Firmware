/**
 * @file time_manager.c
 * @brief Implementación de sincronización y gestión de tiempo para ESP-IDF.
 *
 * Objetivos:
 *  - Sincronizar la hora con NTP.
 *  - Guardar hora UNIX y timestamp de arranque.
 *  - Calcular hora actual estimada usando esp_timer_get_time().
 *  - Permitir trazabilidad entre RTC y uptime.
 */

#include "time_manager.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include <time.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_netif.h" // <-- Añade esta línea
#include <stdlib.h> // Para setenv


static const char *TAG = "time_manager";
static char s_ntp_server[64] = "pool.ntp.org";
static int64_t s_unix_time_synced = 0;         // Hora UNIX sincronizada (segundos)
static int64_t s_boot_time_synced_us = 0;      // esp_timer_get_time() al sincronizar (us)
static bool s_time_synced = false;
static TaskHandle_t s_sync_task_handle = NULL;
static uint32_t s_sync_interval_min = 1440; // 24 horas por defecto
static esp_event_handler_instance_t s_ip_event_handler = NULL;

static void time_sync_notification_cb(struct timeval *tv)
{
    time_t now;
    time(&now);
    s_unix_time_synced = now;
    s_boot_time_synced_us = esp_timer_get_time();
    s_time_synced = true;
    ESP_LOGI(TAG, "Sincronización NTP exitosa. UNIX=%lld, boot_us=%lld", s_unix_time_synced, s_boot_time_synced_us);
}

static void time_manager_ip_event_handler(void* arg, esp_event_base_t event_base,
                                         int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "WiFi reconectado, sincronizando hora NTP...");
        time_manager_sync_ntp();
    }
}

esp_err_t time_manager_init(const char* ntp_server)
{
    // --- Configura la zona horaria para España peninsular (CET/CEST) ---
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();
    // ---------------------------------------------------------------

    if (ntp_server && strlen(ntp_server) < sizeof(s_ntp_server)) {
        strncpy(s_ntp_server, ntp_server, sizeof(s_ntp_server) - 1);
        s_ntp_server[sizeof(s_ntp_server) - 1] = 0;
    }
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, s_ntp_server);
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP inicializado con servidor: %s", s_ntp_server);

    // Primera sincronización NTP (bloqueante, pero solo una vez)
    esp_err_t sync_result = time_manager_sync_ntp();

    // Arrancar sincronización automática usando Kconfig
    time_manager_init_auto_sync_from_kconfig();

    // Registrar manejador de evento para sincronizar al reconectar WiFi
    if (!s_ip_event_handler) {
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                            &time_manager_ip_event_handler, NULL, &s_ip_event_handler);
    }

    return sync_result;
}

esp_err_t time_manager_sync_ntp(void)
{
    s_time_synced = false;
    esp_sntp_restart();
    int retry = 0, retry_count = 10;
    while (!s_time_synced && retry < retry_count) {
        ESP_LOGI(TAG, "Esperando sincronización NTP... (%d/%d)", retry, retry_count);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        retry++;
    }
    if (s_time_synced) {
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "No se pudo sincronizar con NTP");
        return ESP_FAIL;
    }
}

static void time_manager_sync_task(void *arg)
{
    ESP_LOGI(TAG, "time_manager_sync_task watermark=%u",
             uxTaskGetStackHighWaterMark(NULL));
    while (1) {
        // Espera el intervalo configurado
        vTaskDelay(pdMS_TO_TICKS(s_sync_interval_min * 60 * 1000));
        ESP_LOGI(TAG, "Sincronización periódica NTP...");
        time_manager_sync_ntp();
    }
}

esp_err_t time_manager_start_auto_sync(uint32_t interval_min)
{
    if (s_sync_task_handle) return ESP_OK;
    if (interval_min > 0) s_sync_interval_min = interval_min;
    else s_sync_interval_min = 1440; // 24h por defecto

    BaseType_t res = xTaskCreate(
        time_manager_sync_task,
        "time_sync_task",
        2048, // measured usage <1k words
        NULL,
        tskIDLE_PRIORITY + 1,
        &s_sync_task_handle
    );
    return (res == pdPASS) ? ESP_OK : ESP_FAIL;
}

// Llama a esto en tu inicialización principal:
void time_manager_init_auto_sync_from_kconfig(void)
{
    #ifdef CONFIG_TIME_MANAGER_AUTO_SYNC_INTERVAL
    uint32_t interval = CONFIG_TIME_MANAGER_AUTO_SYNC_INTERVAL;
    #else
    uint32_t interval = 1440;
    #endif
    if (interval > 0) {
        time_manager_start_auto_sync(interval);
    }
}

int64_t time_manager_get_unix_time_synced(void)
{
    return s_unix_time_synced;
}

int64_t time_manager_get_boot_time_synced_us(void)
{
    return s_boot_time_synced_us;
}

int64_t time_manager_get_unix_time_now(void)
{
    if (!s_time_synced) return 0;
    int64_t elapsed_us = esp_timer_get_time() - s_boot_time_synced_us;
    return s_unix_time_synced + elapsed_us / 1000000;
}

int64_t time_manager_get_uptime_us(void)
{
    return esp_timer_get_time();
}

int64_t time_manager_get_rtc_vs_uptime_diff(void)
{
    if (!s_time_synced) return 0;
    time_t rtc_now;
    time(&rtc_now);
    int64_t uptime_now = time_manager_get_unix_time_now();
    return (int64_t)rtc_now - uptime_now;
}

esp_err_t time_manager_get_datetime_str(char *buffer, size_t buffer_size, const char *format)
{
    if (!buffer || !format || buffer_size == 0) return ESP_ERR_INVALID_ARG;
    int64_t unix_now = time_manager_get_unix_time_now();
    if (unix_now == 0) return ESP_FAIL;
    time_t t = (time_t)unix_now;
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    if (strftime(buffer, buffer_size, format, &tm_info) == 0) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t time_manager_get_fecha_actual(char *buffer, size_t buffer_size)
{
    // Formato estándar: "YYYY-MM-DD HH:MM:SS"
    return time_manager_get_datetime_str(buffer, buffer_size, "%Y-%m-%d %H:%M:%S");
}

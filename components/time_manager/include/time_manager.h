/**
 * @file time_manager.h
 * @brief Componente para sincronización y gestión de tiempo en ESP-IDF.
 *
 * Objetivos:
 *  - Sincronizar la hora con un servidor NTP (requiere WiFi, gestionado por la app principal).
 *  - Guardar la hora UNIX y el timestamp de arranque al sincronizar.
 *  - Calcular la hora actual usando esp_timer_get_time() y la última sincronización.
 *  - Permitir obtener:
 *      - Hora UNIX sincronizada
 *      - Timestamp de arranque sincronizado
 *      - Hora UNIX actual estimada
 *      - Tiempo desde arranque
 *      - Diferencia entre hora RTC y estimada (para trazabilidad)
 */

#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el sistema de sincronización de tiempo (no conecta WiFi).
 * @param ntp_server Servidor NTP (NULL para usar el predeterminado)
 */
esp_err_t time_manager_init(const char* ntp_server);

/**
 * @brief Sincroniza la hora con el servidor NTP y almacena la hora UNIX y el timestamp de arranque.
 */
esp_err_t time_manager_sync_ntp(void);

/**
 * @brief Devuelve la hora UNIX almacenada tras la última sincronización NTP.
 */
int64_t time_manager_get_unix_time_synced(void);

/**
 * @brief Devuelve el valor de esp_timer_get_time() (us) en el momento de la última sincronización.
 */
int64_t time_manager_get_boot_time_synced_us(void);

/**
 * @brief Devuelve la hora UNIX actual estimada usando la última sincronización y el tiempo desde arranque.
 */
int64_t time_manager_get_unix_time_now(void);

/**
 * @brief Devuelve el tiempo desde arranque en microsegundos.
 */
int64_t time_manager_get_uptime_us(void);

/**
 * @brief Devuelve la diferencia (en segundos) entre la hora RTC del sistema y la estimada por uptime.
 */
int64_t time_manager_get_rtc_vs_uptime_diff(void);

/**
 * @brief Obtiene la fecha y hora actual estimada como cadena formateada.
 * @param buffer Buffer donde se almacenará la cadena.
 * @param buffer_size Tamaño del buffer.
 * @param format Formato de la fecha/hora (ejemplo: "%d/%m/%Y %H:%M:%S").
 * @return
 *   - ESP_OK si la conversión fue exitosa.
 *   - ESP_ERR_INVALID_ARG si los argumentos son inválidos.
 *   - ESP_ERR_NO_MEM si el buffer es muy pequeño.
 */
esp_err_t time_manager_get_datetime_str(char *buffer, size_t buffer_size, const char *format);

/**
 * @brief Devuelve la fecha y hora actual estimada en formato "YYYY-MM-DD HH:MM:SS".
 * @param buffer Buffer donde se almacenará la cadena.
 * @param buffer_size Tamaño del buffer.
 * @return ESP_OK si la conversión fue exitosa, error en caso contrario.
 */
esp_err_t time_manager_get_fecha_actual(char *buffer, size_t buffer_size);

/**
 * @brief Inicia la sincronización automática periódica en background.
 * @param interval_min Intervalo en minutos (ej: 1440 para 24h).
 * @return ESP_OK si la tarea se inició correctamente.
 */
esp_err_t time_manager_start_auto_sync(uint32_t interval_min);

/**
 * @brief Inicia la sincronización automática usando el valor de Kconfig.
 */
void time_manager_init_auto_sync_from_kconfig(void);

#ifdef __cplusplus
}
#endif

#endif // TIME_MANAGER_H

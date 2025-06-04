# Componente time_manager

## Objetivos

- Sincronizar la hora con un servidor NTP (requiere WiFi, gestionado por la app principal).
- Guardar la hora UNIX y el timestamp de arranque al sincronizar.
- Calcular la hora actual usando esp_timer_get_time() y la última sincronización.
- Permitir obtener:
  - Hora UNIX sincronizada
  - Timestamp de arranque sincronizado
  - Hora UNIX actual estimada
  - Tiempo desde arranque
  - Diferencia entre hora RTC y estimada (para trazabilidad)

## API principal

```c
esp_err_t time_manager_init(const char* ntp_server);
esp_err_t time_manager_sync_ntp(void);
int64_t time_manager_get_unix_time_synced(void);
int64_t time_manager_get_boot_time_synced_us(void);
int64_t time_manager_get_unix_time_now(void);
int64_t time_manager_get_uptime_us(void);
int64_t time_manager_get_rtc_vs_uptime_diff(void);
esp_err_t time_manager_get_datetime_str(char *buffer, size_t buffer_size, const char *format);
```

## Ejemplo de uso y explicación de cada función

```c
// Inicializa tras conectar WiFi
time_manager_init("pool.ntp.org");
if (time_manager_sync_ntp() == ESP_OK) {
    // 1. Hora UNIX sincronizada (segundos desde 1970-01-01 UTC)
    int64_t unix_synced = time_manager_get_unix_time_synced();
    printf("Hora sincronizada (UNIX): %lld\n", unix_synced);
    // Ejemplo de salida: 1718041234

    // 2. Timestamp de arranque sincronizado (microsegundos desde boot)
    int64_t boot_synced_us = time_manager_get_boot_time_synced_us();
    printf("Timestamp de arranque sincronizado (us): %lld\n", boot_synced_us);
    // Ejemplo de salida: 51234567

    // 3. Hora UNIX actual estimada (segundos desde 1970-01-01 UTC)
    int64_t unix_now = time_manager_get_unix_time_now();
    printf("Hora actual estimada (UNIX): %lld\n", unix_now);
    // Ejemplo de salida: 1718041240 (va aumentando cada segundo)

    // 4. Tiempo desde arranque (microsegundos)
    int64_t uptime_us = time_manager_get_uptime_us();
    printf("Tiempo desde arranque (us): %lld\n", uptime_us);
    // Ejemplo de salida: 60000000

    // 5. Diferencia entre hora RTC y estimada (segundos)
    int64_t diff = time_manager_get_rtc_vs_uptime_diff();
    printf("Diferencia RTC vs uptime (s): %lld\n", diff);
    // Ejemplo de salida: 0 (si todo está sincronizado), o un valor pequeño si hay drift
```

### ¿Qué hace cada función?

- **time_manager_init(const char* ntp_server)**
  - Inicializa el sistema SNTP con el servidor NTP indicado.
  - No sincroniza la hora aún, solo prepara el sistema.

- **time_manager_sync_ntp(void)**
  - Realiza la sincronización NTP.
  - Al sincronizar, guarda la hora UNIX y el timestamp de arranque actuales.

- **time_manager_get_unix_time_synced(void)**
  - Devuelve la hora UNIX (segundos desde 1970) en el momento de la última sincronización NTP.
  - Ejemplo: `1718041234`

- **time_manager_get_boot_time_synced_us(void)**
  - Devuelve el valor de esp_timer_get_time() (microsegundos desde boot) en el momento de la sincronización.
  - Ejemplo: `51234567`

- **time_manager_get_unix_time_now(void)**
  - Calcula la hora UNIX actual sumando el tiempo transcurrido desde la sincronización.
  - Ejemplo: `1718041240` (va aumentando cada segundo)

- **time_manager_get_uptime_us(void)**
  - Devuelve el tiempo desde arranque en microsegundos.
  - Ejemplo: `60000000`

- **time_manager_get_rtc_vs_uptime_diff(void)**
  - Devuelve la diferencia (en segundos) entre la hora RTC del sistema (`time()`) y la hora estimada por uptime.
  - Útil para detectar drift o problemas de sincronización.
  - Ejemplo: `0` si todo está sincronizado, o un valor pequeño si hay deriva.

- **time_manager_get_datetime_str(char *buffer, size_t buffer_size, const char *format)**
  - Devuelve la fecha y hora actual en una cadena formateada según el formato especificado.
  - Ejemplo: `10/06/2024 15:23:45`

## Ejemplo: Obtener día, mes, año y hora exacta

Puedes convertir la hora UNIX estimada a una estructura `struct tm` y luego a una cadena legible:

```c
#include <time.h>

// ...tras sincronizar...

int64_t unix_now = time_manager_get_unix_time_now();
time_t t = (time_t)unix_now;
struct tm tm_info;
localtime_r(&t, &tm_info);

printf("Fecha y hora exacta: %02d/%02d/%04d %02d:%02d:%02d\n",
    tm_info.tm_mday,
    tm_info.tm_mon + 1,      // tm_mon es 0-11
    tm_info.tm_year + 1900,  // tm_year es años desde 1900
    tm_info.tm_hour,
    tm_info.tm_min,
    tm_info.tm_sec
);
// Ejemplo de salida: 10/06/2024 15:23:45
```

También puedes usar `strftime` para obtener la fecha/hora en el formato que prefieras:

```c
char fecha[32];
strftime(fecha, sizeof(fecha), "%d/%m/%Y %H:%M:%S", &tm_info);
printf("Fecha legible: %s\n", fecha);
// Ejemplo de salida: Fecha legible: 10/06/2024 15:23:45
```

## Ejemplo: Obtener día, mes, año y hora exacta (API)

```c
char fecha[32];
if (time_manager_get_datetime_str(fecha, sizeof(fecha), "%d/%m/%Y %H:%M:%S") == ESP_OK) {
    printf("Fecha legible: %s\n", fecha);
    // Ejemplo de salida: 10/06/2024 15:23:45
} else {
    printf("No disponible (¿sincronización pendiente?)\n");
}
```

## Ejemplo: Obtener la fecha y hora actual como string simple

```c
char fecha_actual[24];
if (time_manager_get_fecha_actual(fecha_actual, sizeof(fecha_actual)) == ESP_OK) {
    printf("Fecha actual: %s\n", fecha_actual);
    // Ejemplo de salida: Fecha actual: 2024-06-10 15:23:45
}
```

## Sincronización automática periódica

El componente puede sincronizar la hora automáticamente cada cierto intervalo (por defecto, cada 24 horas) en segundo plano, sin bloquear el ESP32 ni afectar ISRs.

### ¿Cómo funciona?

- Se crea una tarea FreeRTOS interna que espera el intervalo configurado y ejecuta la sincronización NTP.
- El sistema sigue funcionando normalmente; la sincronización nunca bloquea el loop principal ni ISRs.
- Puedes configurar el intervalo en minutos desde Kconfig o manualmente.

### Ejemplo de uso

```c
// En tu inicialización principal:
time_manager_init("pool.ntp.org");
time_manager_sync_ntp(); // Primera sincronización tras WiFi

// Iniciar sincronización automática cada 24 horas:
time_manager_start_auto_sync(1440); // 1440 minutos = 24 horas

// O usar el valor de Kconfig automáticamente:
time_manager_init_auto_sync_from_kconfig();
```

## Sincronización automática al reconectar WiFi

El componente detecta automáticamente cuando el ESP32 recupera la conexión WiFi (evento IP_EVENT_STA_GOT_IP) y sincroniza la hora con NTP en ese momento, sin intervención de la app principal ni de wifi_sta.

## Uso recomendado

Solo necesitas llamar a la función tras conectar WiFi:

```c
time_manager_init("pool.ntp.org");
```

Esto realiza automáticamente:
- Inicialización SNTP
- Primera sincronización NTP (bloqueante, solo una vez)
- Arranque de la sincronización automática periódica (según Kconfig)

## Zona horaria

Por defecto, el sistema usa UTC. Este componente configura automáticamente la zona horaria de España peninsular (CET/CEST) al inicializarse, por lo que las funciones como `localtime` y `strftime` devolverán la hora local correcta.

Si necesitas otra zona horaria, modifica la línea correspondiente en `time_manager_init`:

```c
setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
tzset();
```

Consulta [la documentación de TZ](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html) para otros formatos.

---

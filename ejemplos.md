I (6183) mqtt_service: MQTT_EVENT_PUBLISHED, msg_id=31196
I (10523) mqtt_service: MQTT_EVENT_DATA
I (10523) mqtt_service: Procesando mensaje en tópico: ota/50787D19F7E0
I (10523) mqtt_service: Mensaje OTA recibido en tópico correcto: ota/50787D19F7E0
I (10533) mqtt_service: Procesando mensaje OTA: {
  "url": "https://github.com/yeerliin/Ecokey_Firmware/releases/download/Test/Ecokey_Firmware.bin"
}
I (10543) mqtt_service: URL de actualización recibida: https://github.com/yeerliin/Ecokey_Firmware/releases/download/Test/Ecokey_Firmware.bin
I (10553) mqtt_service: Iniciando actualización OTA: URL=https://github.com/yeerliin/Ecokey_Firmware/releases/download/Test/Ecokey_Firmware.bin, versión=desconocida, forzar=no
I (10573) ota_service: Partición OTA disponible: ota_0, offset 0x1c0000
I (10573) ota_service: Verificando URL antes de iniciar OTA: https://github.com/yeerliin/Ecokey_Firmware/releases/download/Test/Ecokey_Firmware.bin
I (11543) esp-x509-crt-bundle: Certificate validated
I (12303) ota_service: Respuesta HTTP: 0
I (12303) ota_service: Iniciando OTA desde: https://github.com/yeerliin/Ecokey_Firmware/releases/download/Test/Ecokey_Firmware.bin
I (13253) esp-x509-crt-bundle: Certificate validated
I (14293) esp-x509-crt-bundle: Certificate validated
I (15133) esp_https_ota: Starting OTA...
I (15133) esp_https_ota: Writing to <ota_0> partition at offset 0x1c0000
I (37453) esp_image: segment 0: paddr=001c0020 vaddr=3c0e0020 size=47c3ch (293948) map
I (37493) esp_image: segment 1: paddr=00207c64 vaddr=3fc9fb00 size=05714h ( 22292) 
I (37503) esp_image: segment 2: paddr=0020d380 vaddr=40374000 size=02c98h ( 11416)
I (37503) esp_image: segment 3: paddr=00210020 vaddr=42000020 size=dba68h (899688) map
I (37623) esp_image: segment 4: paddr=002eba90 vaddr=40376c98 size=18e4ch (101964) 
I (37643) esp_image: segment 5: paddr=003048e4 vaddr=600fe100 size=0001ch (    28) 
I (37643) esp_image: segment 0: paddr=001c0020 vaddr=3c0e0020 size=47c3ch (293948) map
I (37683) esp_image: segment 1: paddr=00207c64 vaddr=3fc9fb00 size=05714h ( 22292) 
I (37693) esp_image: segment 2: paddr=0020d380 vaddr=40374000 size=02c98h ( 11416)
I (37693) esp_image: segment 3: paddr=00210020 vaddr=42000020 size=dba68h (899688) map
I (37823) esp_image: segment 4: paddr=002eba90 vaddr=40376c98 size=18e4ch (101964) 
I (37833) esp_image: segment 5: paddr=003048e4 vaddr=600fe100 size=0001ch (    28)
I (37893) ota_service: OTA finalizada correctamente. Reiniciando...
I (37893) mqtt_service: Mensaje enviado al topic ota/status: {"estado":"exito","tipo":"respuesta"} (ID=3844, QoS=1, retain=0)
I (38893) wifi:state: run -> init (0x0)
I (38903) wifi:pm stop, total sleep time: 13532155 us / 36828022 us

I (38903) wifi:<ba-del>idx:0, tid:0
I (38903) wifi:new:<5,0>, old:<5,0>, ap:<255,255>, sta:<5,0>, prof:6, snd_ch_cfg:0x0
W (38913) WIFI_STA: Desconectado del AP, razón: 8
I (38913) WIFI_STA: Iniciando proceso de reconexión automática a SSID: Piso Grande 2.4
I (38943) wifi:flush txq
I (38943) wifi:stop sw txq
I (38943) wifi:lmac stop hw txq
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0xc (RTC_SW_CPU_RST),boot:0x8 (SPI_FAST_FLASH_BOOT)
Saved PC:0x4037f89a
--- 0x4037f89a: esp_cpu_wait_for_intr at C:/Espressif/frameworks/esp-idf-v5.4.1/components/esp_hw_support/cpu.c:64

SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce2810,len:0x15a0
load:0x403c8700,len:0x4
load:0x403c8704,len:0xd20
load:0x403cb700,len:0x2f00
entry 0x403c8928
I (26) boot: ESP-IDF v5.4.1-dirty 2nd stage bootloader
I (26) boot: compile time Jun  6 2025 17:41:44
I (26) boot: Multicore bootloader
I (27) boot: chip revision: v0.2
I (30) boot: efuse block revision: v1.3
I (33) boot.esp32s3: Boot SPI Speed : 80MHz
I (37) boot.esp32s3: SPI Mode       : DIO
I (41) boot.esp32s3: SPI Flash Size : 4MB
I (45) boot: Enabling RNG early entropy source...
I (49) boot: Partition Table:
I (52) boot: ## Label            Usage          Type ST Offset   Length
I (58) boot:  0 nvs              WiFi data        01 02 00009000 00007000
I (65) boot:  1 otadata          OTA data         01 00 00010000 00002000
I (71) boot:  2 phy_init         RF data          01 01 00012000 00001000
I (78) boot:  3 factory          factory app      00 00 00020000 001a0000
I (84) boot:  4 ota_0            OTA app          00 10 001c0000 001a0000
I (91) boot:  5 spiffs           Unknown data     01 82 00360000 000a0000
I (97) boot: End of partition table
I (100) esp_image: segment 0: paddr=001c0020 vaddr=3c0e0020 size=47c3ch (293948) map
I (160) esp_image: segment 1: paddr=00207c64 vaddr=3fc9fb00 size=05714h ( 22292) load
I (165) esp_image: segment 2: paddr=0020d380 vaddr=40374000 size=02c98h ( 11416) load
I (168) esp_image: segment 3: paddr=00210020 vaddr=42000020 size=dba68h (899688) map
I (331) esp_image: segment 4: paddr=002eba90 vaddr=40376c98 size=18e4ch (101964) load
I (353) esp_image: segment 5: paddr=003048e4 vaddr=600fe100 size=0001ch (    28) load
I (364) boot: Loaded app from partition at offset 0x1c0000
I (365) boot: Disabling RNG early entropy source...
I (375) esp_psram: Found 2MB PSRAM device
I (375) esp_psram: Speed: 40MHz
I (375) cpu_start: Multicore app
I (794) esp_psram: SPI SRAM memory test OK
I (803) cpu_start: Pro cpu start user code
I (803) cpu_start: cpu freq: 160000000 Hz
I (803) app_init: Application information:
I (803) app_init: Project name:     Ecokey_Firmware
I (808) app_init: App version:      8f462ca-dirty
I (812) app_init: Compile time:     Jun  6 2025 17:41:18
I (817) app_init: ELF file SHA256:  e2b7a3a87...
--- Warning: Checksum mismatch between flashed and built applications. Checksum of built application is e2c1e6fd0336c115cf52208288e214d872c7e5bd8cdd2898e91b0b6a60f25355
I (822) app_init: ESP-IDF:          v5.4.1-dirty
I (826) efuse_init: Min chip rev:     v0.0
I (830) efuse_init: Max chip rev:     v0.99
I (834) efuse_init: Chip rev:         v0.2
I (838) heap_init: Initializing. RAM available for dynamic allocation:
I (844) heap_init: At 3FCAB2C8 len 0003E448 (249 KiB): RAM
I (849) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
I (854) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (859) heap_init: At 600FE11C len 00001ECC (7 KiB): RTCRAM
I (865) esp_psram: Adding pool of 2048K of PSRAM memory to heap allocator
I (872) spi_flash: detected chip: generic
I (875) spi_flash: flash io: dio
I (878) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (884) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (891) coexist: coex firmware version: e727207
I (895) coexist: coexist rom version e7ae62f
I (900) main_task: Started on CPU0
I (910) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (910) main_task: Calling app_main()
I (910) MAIN: Iniciando aplicación EcoKey
I (920) NVS_MANAGER: Inicializando NVS con namespace: ecokey
I (950) NVS_MANAGER: NVS inicializado correctamente
I (1450) APP_INIT: Inicializando LED...
I (1450) gpio: GPIO[4]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1450) LED: LED inicializado en GPIO: 4, Activo ALTO
I (1450) LED: Parpadeo iniciado con intervalo de 100 ms
I (1460) APP_INIT: Inicializando Relay Controller...
I (1460) gpio: GPIO[18]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (1470) RELAY_CONTROLLER: Relay controller inicializado en GPIO 18, activo en ALTO
I (1580) APP_INIT: Inicializando control por boton...
I (1580) CONTROL_BUTTON: Inicializando lógica de botón
I (1580) gpio: GPIO[13]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1580) BOTON: Botón inicializado en GPIO 13 (nivel: bajo)
E (1700) esp-tls-mbedtls: mbedtls_x509_crt_parse was partly successful. No. of failed certificates: 1
E (1700) OTA: Error al cargar CA store: ESP_ERR_MBEDTLS_CERT_PARTLY_OK
I (1800) APP_INIT: Todos los componentes inicializados correctamente
I (1800) MAIN: Inicialización completada, iniciando estado de la aplicación
I (1800) NVS_MANAGER: La clave 'ssid' existe en NVS como string
I (1800) NVS_MANAGER: Credenciales WiFi encontradas en NVS
I (1810) APP_CONTROL: [BOOT] Primer arranque o reinicio → Estado 3
I (1810) APP_CONTROL: Inicializando servicios de conectividad para estado 3
I (1820) pp: pp rom version: e7ae62f
I (1820) net80211: net80211 rom version: e7ae62f
I (1840) wifi:wifi driver task: 3fcc08ec, prio:23, stack:6656, core=0
I (1850) wifi:wifi firmware version: 79fa3f41ba
I (1850) wifi:wifi certification version: v7.0
I (1850) wifi:config NVS flash: enabled
I (1850) wifi:config nano formatting: disabled
I (1860) wifi:Init data frame dynamic rx buffer num: 32
I (1860) wifi:Init static rx mgmt buffer num: 5
I (1860) wifi:Init management short buffer num: 32
I (1870) wifi:Init dynamic tx buffer num: 32
I (1870) wifi:Init static tx FG buffer num: 2
I (1880) wifi:Init static rx buffer size: 1600
I (1880) wifi:Init static rx buffer num: 10
I (1890) wifi:Init dynamic rx buffer num: 32
I (1890) wifi_init: rx ba win: 6
I (1890) wifi_init: accept mbox: 6
I (1900) wifi_init: tcpip mbox: 32
I (1900) wifi_init: udp mbox: 6
I (1900) wifi_init: tcp mbox: 6
I (1900) wifi_init: tcp tx win: 5760
I (1910) wifi_init: tcp rx win: 5760
I (1910) wifi_init: tcp mss: 1440
I (1910) wifi_init: WiFi IRAM OP enabled
I (1920) wifi_init: WiFi RX IRAM OP enabled
I (1920) phy_init: phy_version 700,8582a7fd,Feb 10 2025,20:13:11
I (1960) wifi:mode : sta (50:78:7d:19:f7:e0)
I (1960) wifi:enable tsf
I (1960) WIFI_STA: WiFi en modo estación inicializado correctamente
I (1960) WIFI_STA: WiFi iniciado, intentando conectar
I (1970) NVS_MANAGER: La clave 'ssid' existe en NVS como string
I (1980) wifi:new:<5,0>, old:<1,0>, ap:<255,255>, sta:<5,0>, prof:6, snd_ch_cfg:0x0
I (1980) wifi:state: init -> auth (0xb0)
I (2000) NVS_MANAGER: Credenciales WiFi encontradas en NVS
I (2000) NVS_MANAGER: La clave 'ssid' existe en NVS como string
I (2000) NVS_MANAGER: La clave 'password' existe en NVS como string
I (2000) wifi:state: auth -> assoc (0x0)
I (2010) NVS_MANAGER: Credenciales WiFi obtenidas correctamente. SSID: Piso Grande 2.4
I (2010) wifi:state: assoc -> run (0x10)
I (2020) wifi:<ba-add>idx:0 (ifx:0, d4:60:e3:5e:24:a1), tid:0, ssn:0, winSize:64
I (2020) WIFI_STA: Conectando con credenciales de NVS. SSID: Piso Grande 2.4
I (2030) WIFI_STA: Conectando a SSID: Piso Grande 2.4
I (2040) wifi:connected with Piso Grande 2.4, aid = 12, channel 5, BW20, bssid = d4:60:e3:5e:24:a1
I (2050) wifi:security: WPA2-PSK, phy: bgn, rssi: -75
I (2050) wifi:pm start, type: 1

I (2050) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (2060) wifi:set rx beacon pti, rx_bcn_pti: 14, bcn_timeout: 25000, mt_pti: 14, mt_time: 10000
I (2040) WIFI_STA: Proceso de reconexión detenido
I (2070) WIFI_STA: Conectado al AP
W (2080) wifi:sta is connected, disconnect before connecting to new ap
I (2080) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (3110) esp_netif_handlers: sta ip: 192.168.0.33, mask: 255.255.255.0, gw: 192.168.0.1
I (3110) WIFI_STA: IP obtenida:192.168.0.33
I (3110) WIFI_STA: Conectado al AP SSID: Piso Grande 2.4
I (3110) time_manager: SNTP inicializado con servidor: pool.ntp.org
I (3120) time_manager: Esperando sincronización NTP... (0/10)
I (4120) time_manager: Esperando sincronización NTP... (1/10)
I (5120) time_manager: Esperando sincronización NTP... (2/10)
I (5320) time_manager: Sincronización NTP exitosa. UNIX=1749228534, boot_us=4453306
I (6120) mqtt_service: Other event id:7
I (6120) NVS_MANAGER: Haciendo commit de los cambios para la clave 'app_estado' con valor 3
I (6120) NVS_MANAGER: Commit exitoso para la clave 'app_estado'
W (6190) esp-tls-mbedtls: mbedtls_x509_crt_parse was partly successful. No. of failed certificates: 1
I (6780) mqtt_service: Suscripción al tópico dispositivos/50787D19F7E0 con QoS 1 (ID=48596)
I (6780) mqtt_service: Suscripción al tópico ota/50787D19F7E0 con QoS 1 (ID=53176)
I (6790) mqtt_service: Suscrito a tópicos de dispositivo y OTA
I (6790) mqtt_service: Mensaje enviado al topic dispositivos/50787D19F7E0/modo: {"Modo":"automatico","FechaModo":"2025-06-06 18:48:54"} (ID=62154, QoS=2, retain=1)
Fecha actual: 2025-06-06 18:48:54
I (6810) LED: Deteniendo parpadeo...
I (6810) LED: Parpadeo detenido
I (6820) ESTADO_AUTO: Iniciando el modo automático
I (6820) mqtt_service: MQTT_EVENT_SUBSCRIBED, msg_id=48596
I (6820) ESTADO_AUTO: MAC objetivo cargada de NVS: [FF:FF:11:9A:FF:CB]
I (6830) ESTADO_AUTO: Configurando escáner BLE para MAC objetivo: FF:FF:11:9A:FF:CB
I (6840) BLE_SCANNER: MAC a buscar (original): FF:FF:11:9A:FF:CB
I (6840) BLE_SCANNER: MAC convertida: FF:FF:11:9A:FF:CB
I (6850) BLE_SCANNER: MAC objetivo #0 configurada: FF:FF:11:9A:FF:CB
I (6860) mqtt_service: MQTT_EVENT_SUBSCRIBED, msg_id=53176
I (6860) BLE_SCANNER: Inicializando BLE Scanner...
I (6870) BLE_SCANNER: Inicializando sensor de temperatura...
I (6870) temperature_sensor: Range [-10°C ~ 80°C], error < 1°C
I (6880) BLE_SCANNER: Sensor de temperatura inicializado correctamente
I (6880) BLE_SCANNER: Tarea de monitoreo térmico iniciada
I (6890) mqtt_service: Mensaje enviado al topic dispositivos/50787D19F7E0/temp: {"temperatura":49.2} (ID=27268, QoS=2, retain=1)
I (6900) BLE_INIT: BT controller compile version [d74042a]
I (6900) mqtt_service: MQTT_EVENT_PUBLISHED, msg_id=62154
I (6910) BLE_INIT: Bluetooth MAC: 50:78:7d:19:f7:e2
I (6920) BLE_SCANNER: BLE Host Task iniciada
I (6920) NimBLE: GAP procedure initiated: stop advertising.

I (6920) NimBLE: Failed to restore IRKs from store; status=8

I (6930) BLE_SCANNER: Host BLE sincronizado, listo para iniciar escaneo
W (6940) BLE_SCANNER: Host sincronizado pero escáner no inicializado completamente, postergando escaneo
I (6950) BLE_SCANNER: Host ya sincronizado, iniciando escaneo inmediatamente
I (6950) BLE_SCANNER: Iniciando escaneo BLE en modo térmico normal
I (6960) NimBLE: GAP procedure initiated: discovery;
I (6960) NimBLE: own_addr_type=0 filter_policy=0 passive=1 limited=0 filter_duplicates=0 
I (6970) NimBLE: duration=forever
I (6970) NimBLE:

I (6980) BLE_SCANNER: Escaneo BLE iniciado con parámetros para modo térmico 0
I (6980) mqtt_service: MQTT_EVENT_PUBLISHED, msg_id=27268
I (6990) BLE_SCANNER: BLE scanner inicializado con éxito
I (6990) BLE_SCANNER: Control térmico: Activado (Warning: 60.0°C, Critical: 70.0°C)
I (7000) ESTADO_AUTO: Timeout de ausencia BLE configurado en 10 minutos (600000 ms)
I (7010) BLE_SCANNER: Escaneo BLE detenido
I (7010) BLE_SCANNER: Iniciando escaneo BLE en modo térmico normal
I (7020) NimBLE: GAP procedure initiated: discovery;
I (7020) NimBLE: own_addr_type=0 filter_policy=0 passive=1 limited=0 filter_duplicates=0 
I (7030) NimBLE: duration=forever
I (7030) MAIN: Aplicación en ejecución con estado inicial activado
I (7030) NimBLE:

I (7040) BLE_SCANNER: Escaneo BLE iniciado con parámetros para modo térmico 0
I (7050) ESTADO_AUTO: Escaneo BLE activado (relé apagado)
I (7050) main_task: Returned from app_main()
I (8920) BLE_SCANNER: Tag #0 detectado! MAC: FF:FF:11:9A:FF:CB RSSI:169
I (9050) mqtt_service: Mensaje enviado al topic dispositivos/50787D19F7E0/estado: {"Estado":"Encendido","Fecha":"2025-06-06 18:48:57"} (ID=1406, QoS=2, retain=1)
Fecha actual: 2025-06-06 18:48:57
I (9060) ESTADO_AUTO: Relé activado por detección BLE
I (9060) BLE_SCANNER: Escaneo BLE detenido
I (9130) mqtt_service: MQTT_EVENT_PUBLISHED, msg_id=1406
I (12890) mqtt_service: Mensaje enviado al topic dispositivos/50787D19F7E0/temp: {"temperatura":47.2} (ID=40377, QoS=2, retain=1)
I (12970) mqtt_service: MQTT_EVENT_PUBLISHED, msg_id=40377
I (39390) mqtt_service: Mensaje enviado al topic dispositivos/50787D19F7E0/temp: {"temperatura":45.2} (ID=33111, QoS=2, retain=1)
I (39470) mqtt_service: MQTT_EVENT_PUBLISHED, msg_id=33111
I (45710) mqtt_service: MQTT_EVENT_DATA
I (45710) mqtt_service: Procesando mensaje en tópico: ota/50787D19F7E0
I (45710) mqtt_service: Mensaje OTA recibido en tópico correcto: ota/50787D19F7E0
I (45720) mqtt_service: Procesando mensaje OTA: {
  "url": "https://github.com/yeerliin/Ecokey_Firmware/releases/download/Test/Ecokey_Firmware.bin"
}
I (45730) mqtt_service: URL de actualización recibida: https://github.com/yeerliin/Ecokey_Firmware/releases/download/Test/Ecokey_Firmware.bin
I (45740) mqtt_service: Iniciando actualización OTA: URL=https://github.com/yeerliin/Ecokey_Firmware/releases/download/Test/Ecokey_Firmware.bin, versión=desconocida, forzar=no
I (45760) ota_service: Verificando URL antes de iniciar OTA: https://github.com/yeerliin/Ecokey_Firmware/releases/download/Test/Ecokey_Firmware.bin
I (46730) esp-x509-crt-bundle: Certificate validated
I (47490) ota_service: Respuesta HTTP: 0
I (47500) ota_service: Iniciando OTA desde: https://github.com/yeerliin/Ecokey_Firmware/releases/download/Test/Ecokey_Firmware.bin
I (48440) esp-x509-crt-bundle: Certificate validated
I (49480) esp-x509-crt-bundle: Certificate validated
I (50320) esp_https_ota: Starting OTA...
I (50320) esp_https_ota: Writing to <ota_0> partition at offset 0x1c0000
E (50320) esp_https_ota: esp_ota_begin failed (ESP_ERR_OTA_PARTITION_CONFLICT)
E (50330) ota_service: Fallo OTA: ESP_ERR_OTA_PARTITION_CONFLICT
I (50340) mqtt_service: Mensaje enviado al topic ota/status: {"estado":"error","mensaje":"OTA fallida","tipo":"respuesta"} (ID=45692, QoS=1, retain=0)
I (50350) mqtt_service: Mensaje enviado al topic ota/50787D19F7E0: {"estado":"iniciando","version":"desconocida","tipo":"respuesta"} (ID=52710, QoS=1, retain=0)
I (50360) mqtt_service: Mensaje enviado al topic dispositivos/50787D19F7E0/temp: {"temperatura":47.2} (ID=33813, QoS=2, retain=1)
I (50380) mqtt_service: MQTT_EVENT_PUBLISHED, msg_id=45692
I (50410) mqtt_service: MQTT_EVENT_PUBLISHED, msg_id=52710
I (50420) mqtt_service: MQTT_EVENT_DATA
I (50420) mqtt_service: Procesando mensaje en tópico: ota/50787D19F7E0
I (50460) mqtt_service: MQTT_EVENT_PUBLISHED, msg_id=33813
I (62390) mqtt_service: Mensaje enviado al topic dispositivos/50787D19F7E0

Teniendo en cuenta la memoria del codigo tal vez no es mejor crear otra particion y alternar entre esas dos o da igual si utilizo la factory? El ESP32 S3 es Built-in 512KB of SRAM, 384KB ROM, 16 KB RTC SRAM, 2MB PSRAM and 4MB Flash memory.

┏━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┓ ┃ Memory Type/Section ┃ Used [bytes] ┃ Used [%] ┃ Remain [bytes] ┃ Total [bytes] ┃ ┡━━━━━━━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━╇━━━━━━━━━━╇━━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━━┩ │ Flash Code │ 900598 │ 10.74 │ 7487978 │ 8388576 │ │ .text │ 900598 │ 10.74 │ │ │ │ Flash Data │ 294444 │ 0.88 │ 33259956 │ 33554400 │ │ .rodata │ 294188 │ 0.88 │ │ │ │ .appdesc │ 256 │ 0.0 │ │ │ │ DIRAM │ 144068 │ 42.15 │ 197692 │ 341760 │ │ .text │ 96995 │ 28.38 │ │ │ │ .bss │ 24752 │ 7.24 │ │ │ │ .data │ 22292 │ 6.52 │ │ │ │ IRAM │ 16383 │ 99.99 │ 1 │ 16384 │ │ .text │ 15356 │ 93.73 │ │ │ │ .vectors │ 1027 │ 6.27 │ │ │ │ RTC FAST │ 308 │ 3.76 │ 7884 │ 8192 │ │ .force_fast │ 28 │ 0.34 │ │ │ │ .rtc_reserved │ 24 │ 0.29 │ │ │ └─────────────────────┴──────────────┴──────────┴────────────────┴───────────────┘ Total image size: 1330740 bytes (.bin may be padded larger)

partitions.csv
# Name,     Type, SubType, Offset,    Size,      Flags
nvs,        data, nvs,     0x9000,    0x7000,
otadata,    data, ota,     0x10000,   0x2000,
phy_init,   data, phy,     0x12000,   0x1000,
factory,    app,  factory, 0x20000,   0x1A0000,
ota_0,      app,  ota_0,   0x1C0000,  0x1A0000,
spiffs,     data, spiffs,  0x360000,  0xA0000,
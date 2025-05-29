idf_component_register(
    SRCS "estado_automatico.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer freertos ble_scanner relay_controller nvs_manager mqtt_service wifi_sta time_manager
)

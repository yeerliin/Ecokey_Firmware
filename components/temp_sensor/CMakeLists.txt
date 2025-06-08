idf_component_register(
    SRCS "temp_sensor.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash mqtt_service driver esp_timer
)

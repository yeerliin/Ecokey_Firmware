idf_component_register(
    SRCS "estado_manual.c"
    INCLUDE_DIRS "include"
    REQUIRES relay_controller mqtt_service wifi_sta time_manager led nvs_manager resource_manager
)

idf_component_register(
    SRCS "app_control.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_manager estado_automatico estado_manual estado_configuracion wifi_sta mqtt_service time_manager 
)

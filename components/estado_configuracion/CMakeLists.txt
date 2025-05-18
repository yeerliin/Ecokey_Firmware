idf_component_register(
    SRCS "estado_configuracion.c"
    INCLUDE_DIRS "include"
    REQUIRES app_control  wifi_provision_web led nvs_manager
)

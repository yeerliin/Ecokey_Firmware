idf_component_register(
    SRCS "app_control.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_manager estado_inicio estado_automatico estado_manual estado_configuracion
)

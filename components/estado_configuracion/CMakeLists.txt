idf_component_register(
    SRCS "estado_configuracion.c"
    INCLUDE_DIRS "include"
    REQUIRES app_control led provisioning_wifi
)

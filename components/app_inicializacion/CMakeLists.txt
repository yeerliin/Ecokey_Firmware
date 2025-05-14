idf_component_register(
    SRCS "app_inicializacion.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_manager ble_scanner app_control control_button led relay_controller
)
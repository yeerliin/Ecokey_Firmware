idf_component_register(
    SRCS "app_inicializacion.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_manager ble_scanner app_control control_button led relay_controller
)

# 🔴 ¡ESTA línea va fuera del bloque anterior!
target_add_binary_data(${COMPONENT_TARGET} "${CMAKE_SOURCE_DIR}/certs/ca.pem" TEXT)


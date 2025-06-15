idf_component_register(
    SRCS "resource_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common heap freertos
)

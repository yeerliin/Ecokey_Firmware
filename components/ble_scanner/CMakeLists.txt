idf_component_register(SRCS "ble_scanner.c"
                      INCLUDE_DIRS "include"
                      REQUIRES bt esp_common driver mqtt_service)
idf_component_register(SRCS "mqtt_client.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_event mqtt app_control relay_controller nvs_manager)
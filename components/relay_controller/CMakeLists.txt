idf_component_register(SRCS "relay_controller.c"
                      INCLUDE_DIRS "include"
                      REQUIRES driver mqtt_service wifi_sta time_manager app_control)
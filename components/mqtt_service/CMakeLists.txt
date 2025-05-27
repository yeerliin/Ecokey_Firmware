idf_component_register(SRCS "mqtt_service.c"
                      INCLUDE_DIRS "include"
                      REQUIRES nvs_flash esp_event mqtt json esp_netif wifi_sta)

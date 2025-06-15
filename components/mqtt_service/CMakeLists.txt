idf_component_register(SRCS "mqtt_service.c"
                      INCLUDE_DIRS "include"
                      REQUIRES nvs_flash esp_event mqtt json esp_netif wifi_sta ble_scanner relay_controller app_control ota_service
                      )

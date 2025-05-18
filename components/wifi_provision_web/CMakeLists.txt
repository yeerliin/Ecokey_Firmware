idf_component_register(SRCS "wifi_provision_web.c"
                      "wifi_provision_web_form.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_manager json esp_wifi esp_http_server)

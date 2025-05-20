idf_component_register(SRCS "control_button.c"
                    INCLUDE_DIRS "include"
                    REQUIRES button app_control estado_manual relay_controller nvs_manager)

idf_component_register(SRCS "app_wifi.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi nvs_flash esp_event driver)

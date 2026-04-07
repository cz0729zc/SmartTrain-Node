idf_component_register(SRCS "driver_co2.c" "driver_dht.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_timer)

idf_component_register(SRCS "app_bootstrap.c" "app_perf_monitor.c" "app_face.c" "app_fingerprint.c" "app_lvgl.c" "app_display.c" "app_co2.c" "app_rfid.c" "app_time.c" "attendance_profile.c" "attendance_record.c" "app_attendance.c" "app_test_hub.c" "main.c" "app_sensor.c" "app_network.c" "sensor_data.c"
                    INCLUDE_DIRS "." "include"
                    REQUIRES app_wifi app_mqtt bsp common drivers ui_custom
                    PRIV_REQUIRES spiffs lvgl__lvgl espressif__esp_lvgl_port abobija__rc522 nvs_flash esp_netif esp_timer esp_wifi esp_event driver)

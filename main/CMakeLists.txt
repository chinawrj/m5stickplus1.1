idf_component_register(SRCS "page_manager_espnow.c" "page_manager_monitor.c" "button.c" "red_led.c" "buzzer.c" "st7789v2.c" "power_safety_demo.c" "espnow_example_main.c" "axp192.c" "st7789_lcd.c" "lvgl_demo_ui.c" "lvgl_init.c" "system_monitor.c" "page_manager.c" "lvgl_button_input.c" "page_manager_lvgl.c" "ux_service.c" "espnow_manager.c"
                    PRIV_REQUIRES nvs_flash esp_event esp_netif esp_wifi
                                  esp_driver_gpio esp_driver_i2c esp_driver_spi esp_lcd esp_driver_ledc esp_timer lvgl__lvgl
                    INCLUDE_DIRS ".")

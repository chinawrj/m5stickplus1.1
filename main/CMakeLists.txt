idf_component_register(SRCS "espnow_example_main.c" "axp192.c" "st7789_driver.c"
                    PRIV_REQUIRES nvs_flash esp_event esp_netif esp_wifi
                                  esp_driver_gpio esp_driver_i2c esp_driver_spi
                                  esp_lcd
                    INCLUDE_DIRS ".")

idf_component_register(
    SRCS "world.c" "gfx.c" "scenes.c" "clock.c" "app_main.cpp"
         "panel.cpp"
         "font.c"
         "wifi.c"
         "daily.c"
         "encoder.c"
         "als.c"
         "audio.c"
         "mp3_impl.c"
         "apod.c"
         "ota.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        esp_http_client
        esp_https_ota
        app_update
        esp-tls
        nvs_flash
        lwip
        mbedtls
        esp_driver_i2c
        esp_driver_i2s
)

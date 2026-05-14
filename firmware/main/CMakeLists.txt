idf_component_register(
    SRCS "world.c" "gfx.c" "scenes.c" "clock.c" "app_main.cpp"
         "panel.cpp"
         "font.c"
         "wifi.c"
         "daily.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        esp_http_client
        esp-tls
        nvs_flash
        lwip
        mbedtls
)

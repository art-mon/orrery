idf_component_register(
    SRCS "app_main.cpp"
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
)

idf_component_register(
    SRCS main.c ecran_ui.c
    INCLUDE_DIRS .
    REQUIRES commun esp_wifi esp_wifi_remote esp_event esp_netif nvs_flash
    PRIV_REQUIRES espressif__esp32_p4_function_ev_board)

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE 
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -DLV_USE_DEMO_MUSIC
)

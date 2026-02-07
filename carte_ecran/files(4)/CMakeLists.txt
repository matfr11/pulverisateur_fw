idf_component_register(
    SRCS main.c ui_pulverisateur.c mqtt_ecran.c
    INCLUDE_DIRS .
    REQUIRES nvs_flash esp_wifi esp_event esp_netif mqtt json
)

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB}
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
)

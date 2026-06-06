idf_component_register(
    SRCS "gestion_ota.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/main"
    REQUIRES esp_http_server app_update esp_timer
)

idf_component_register(
    SRCS "gestion_capteurs.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/main"
    REQUIRES driver esp_adc commun
)
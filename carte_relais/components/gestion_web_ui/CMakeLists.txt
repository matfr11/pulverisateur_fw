idf_component_register(
    SRCS "gestion_web_ui.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/main"
    REQUIRES commun gestion_actionneurs gestion_capteurs gestion_automatismes 
             gestion_securites gestion_configuration esp_http_server json
)

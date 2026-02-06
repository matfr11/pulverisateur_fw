idf_component_register(
    SRCS "gestion_automatismes.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/main"
    REQUIRES commun gestion_actionneurs gestion_capteurs
)

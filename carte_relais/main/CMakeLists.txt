# CMakeLists.txt - Composant main
# Point d'entrée de l'application
idf_component_register(
    SRCS 
        "app_main.cpp"
        "app_init.cpp"
        "app_role.cpp"
    INCLUDE_DIRS 
        "."
        "${CMAKE_CURRENT_LIST_DIR}/../../commun/include"
    REQUIRES 
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        esp_timer
        driver
        freertos
        cjson
        gestion_mqtt
        gestion_actionneurs
        gestion_capteurs
        gestion_http
        gestion_configuration
        gestion_wifi
)
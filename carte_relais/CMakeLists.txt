# Projet ESP-IDF : Carte Relais (AVANT ou ARRIÈRE)
# La carte est sélectionnée dans main/board_config.h
cmake_minimum_required(VERSION 3.16)

# Inclure le code commun partagé
set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_CURRENT_SOURCE_DIR}/../commun"
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(carte_relais)

# CMakeLists.txt - Projet carte_relais (AVANT et ARRIERE)
# Système pulvérisateur agricole
# Version 1.0 - 2026-02-02

cmake_minimum_required(VERSION 3.16)

# ============================================================================
# SÉLECTION DU TYPE DE CARTE
# ============================================================================
# Utilisation : 
#   idf.py -D BOARD_TYPE=AVANT build
#   idf.py -D BOARD_TYPE=ARRIERE build
# ============================================================================

if(NOT DEFINED BOARD_TYPE)
    message(FATAL_ERROR 
        "BOARD_TYPE non défini!\n"
        "Utilisez: idf.py -D BOARD_TYPE=AVANT build\n"
        "      ou: idf.py -D BOARD_TYPE=ARRIERE build")
endif()

if(BOARD_TYPE STREQUAL "AVANT")
    message(STATUS ">>> Configuration pour CARTE AVANT <<<")
    add_compile_definitions(BOARD_TYPE_AVANT)
    set(PROJECT_NAME "pulverisateur_avant")
elseif(BOARD_TYPE STREQUAL "ARRIERE")
    message(STATUS ">>> Configuration pour CARTE ARRIERE <<<")
    add_compile_definitions(BOARD_TYPE_ARRIERE)
    set(PROJECT_NAME "pulverisateur_arriere")
else()
    message(FATAL_ERROR "BOARD_TYPE invalide: ${BOARD_TYPE}. Valeurs acceptées: AVANT, ARRIERE")
endif()

# ============================================================================
# OPTIONS DE COMPILATION
# ============================================================================

# Mode simulation (optionnel)
option(MODE_SIMULATION "Activer le mode simulation (capteurs/actionneurs virtuels)" OFF)
if(MODE_SIMULATION)
    message(STATUS "Mode SIMULATION activé")
    add_compile_definitions(CONFIG_MODE_SIMULATION)
endif()

# Niveau de log
set(LOG_LEVEL "INFO" CACHE STRING "Niveau de log (ERROR, WARN, INFO, DEBUG)")
if(LOG_LEVEL STREQUAL "DEBUG")
    add_compile_definitions(CONFIG_LOG_LEVEL_DEBUG)
endif()

# ============================================================================
# CHEMINS ADDITIONNELS
# ============================================================================

# Ajouter le dossier commun au chemin d'inclusion
set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_CURRENT_LIST_DIR}/../commun"
    "${CMAKE_CURRENT_LIST_DIR}/components"
)

# ============================================================================
# PROJET ESP-IDF
# ============================================================================

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(${PROJECT_NAME})

# ============================================================================
# INFORMATIONS DE VERSION
# ============================================================================

set(VERSION_MAJEURE 1)
set(VERSION_MINEURE 0)
set(VERSION_PATCH 0)

add_compile_definitions(
    VERSION_MAJEURE=${VERSION_MAJEURE}
    VERSION_MINEURE=${VERSION_MINEURE}
    VERSION_PATCH=${VERSION_PATCH}
)

message(STATUS "Version: ${VERSION_MAJEURE}.${VERSION_MINEURE}.${VERSION_PATCH}")
message(STATUS "IDF_PATH: $ENV{IDF_PATH}")
message(STATUS "IDF_TARGET: ${IDF_TARGET}")

# ============================================================================
# WARNINGS ET OPTIMISATIONS
# ============================================================================

# Activer tous les warnings
add_compile_options(-Wall -Wextra -Werror=return-type)

# Optimisation
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-O0 -g3)
    message(STATUS "Build type: Debug (optimisation désactivée)")
else()
    add_compile_options(-O2)
    message(STATUS "Build type: Release (optimisation O2)")
endif()

# ============================================================================
# POST-BUILD: Informations de taille
# ============================================================================

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_SIZE} ${PROJECT_NAME}.elf
    COMMENT "Taille du firmware:"
)

# ============================================================================
# CONFIGURATION FINALE
# ============================================================================

message(STATUS "==============================================")
message(STATUS "  Configuration du projet:")
message(STATUS "  - Nom: ${PROJECT_NAME}")
message(STATUS "  - Type carte: ${BOARD_TYPE}")
message(STATUS "  - Version: ${VERSION_MAJEURE}.${VERSION_MINEURE}.${VERSION_PATCH}")
message(STATUS "  - Simulation: ${MODE_SIMULATION}")
message(STATUS "  - Log level: ${LOG_LEVEL}")
message(STATUS "==============================================")

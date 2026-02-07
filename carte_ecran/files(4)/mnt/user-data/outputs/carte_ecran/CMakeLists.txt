# Carte Écran Pulvérisateur
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS
    ../common_components
)

add_compile_options(-Wno-format)
project(pulve_ecran)

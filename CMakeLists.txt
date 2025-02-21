# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Définir le chemin vers les composants supplémentaires d'ESP-ADF
set(EXTRA_COMPONENT_DIRS $ENV{ADF_PATH}/components)

# Inclure le CMakeLists.txt d'ESP-ADF
include($ENV{ADF_PATH}/CMakeLists.txt)

# Inclure les outils CMake d'ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Nom du projet
project(udp_audio)
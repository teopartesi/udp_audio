# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/teopa/Espressif/frameworks/esp-idf-v4.4.4/components/bootloader/subproject"
  "C:/Users/teopa/esp32/udp_audio/build/bootloader"
  "C:/Users/teopa/esp32/udp_audio/build/bootloader-prefix"
  "C:/Users/teopa/esp32/udp_audio/build/bootloader-prefix/tmp"
  "C:/Users/teopa/esp32/udp_audio/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/teopa/esp32/udp_audio/build/bootloader-prefix/src"
  "C:/Users/teopa/esp32/udp_audio/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/teopa/esp32/udp_audio/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()

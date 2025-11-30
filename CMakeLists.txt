cmake_minimum_required(VERSION 3.10)
project(photon VERSION 0.0.1 LANGUAGES C)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
message("Generating ${CMAKE_BUILD_TYPE} build files for photon")

add_executable(photon
    src/main.c src/photon.h
    src/ui.c src/ui.h
    src/photon_debug.c src/photon_debug.h
    src/input.c src/input.h
    src/buffer.c src/buffer.h
    src/extensions.c src/extensions.h
)

set_target_properties(photon PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED NO
)

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    string(LENGTH "${PROJECT_SOURCE_DIR}/src" SRC_LEN)
    target_compile_definitions(photon PRIVATE PHOTON_DEBUG=1 PHOTON_DB_CODE_ROOT_LEN=${SRC_LEN} ${USER_DEBUG_OPTIONS})
else()
    target_compile_options(photon PRIVATE "-O2")
endif()

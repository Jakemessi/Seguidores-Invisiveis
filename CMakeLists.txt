# This file configures the "build" of this SKSE plugin
# - how to compile the C++ plugin (in addition to settings found in CMakePresets.json)
# - where to output the resulting .dll file

# It's recommended to set a minimum CMake version.
# If you use CMake features from higher versions, update this to match.
cmake_minimum_required(VERSION 3.21)

# Set your project name. This will be the name of your SKSE .dll file.
project(InvisibilityAffectsFollowersToo VERSION 2.0.0.0 LANGUAGES CXX RC)

set(SKSSE_PLUGIN_AUTHOR "Huehuller")
set(SKSSE_PLUGIN_EMAIL "nanaveine@gmail.com")

set(OUTPUT_FOLDER "C:\\Users\\Huehuller\\Documents\\VSCode Projects\\Seguidores-Invisiveis\\")

# Gera Version.h para o C++
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h"
    @ONLY
)

# Gera PluginVersion.rc para o Windows Explorer
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginVersion.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/PluginVersion.rc"
    @ONLY
)

find_package(CommonLibSSE CONFIG REQUIRED)

add_commonlibsse_plugin(${PROJECT_NAME}
    AUTHOR ${SKSSE_PLUGIN_AUTHOR}
    EMAIL ${SKSSE_PLUGIN_EMAIL}
    SOURCES
        plugin.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/generated/PluginVersion.rc"
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE PCH.h)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

if(DEFINED OUTPUT_FOLDER)
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
            VERBATIM
        )
    endif()
endif()
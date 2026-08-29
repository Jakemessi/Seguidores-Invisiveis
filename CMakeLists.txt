# This file configures the "build" of this SKSE plugin
# - how to compile the C++ plugin (in addition to settings found in CMakePresets.json)
# - where to output the resulting .dll file

# It's recommended to set a minimum CMake version.
# If you use CMake features from higher versions, update this to match.
cmake_minimum_required(VERSION 3.21)

if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
else()
    message(WARNING "VCPKG_ROOT não está definido. O CMake pode não encontrar as dependências.")
endif()

# Set your project name. This will be the name of your SKSE .dll file.
project(InvisibilityAffectsFollowersToo VERSION 5.4.3.0 LANGUAGES CXX RC)
# Major . Minor . Patch . Tweak

set(SKSSE_PLUGIN_AUTHOR "Huehuller")
set(SKSSE_PLUGIN_EMAIL "nanaveine@gmail.com")

set(OUTPUT_FOLDER "C:\\Users\\Huehuller\\Documents\\VSCode Projects\\Seguidores-Invisiveis\\")

find_package(CommonLibSSE CONFIG REQUIRED)

add_compile_definitions(
    SKSE_SUPPORT_XBYAK
)

# Gera Version.h para o C++
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h"
    @ONLY
)

# Gera PluginVersion.rc para o Windows Explorer
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PluginVersion.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/PluginVersion.rc"
    @ONLY
)

add_commonlibsse_plugin(${PROJECT_NAME}
    AUTHOR ${SKSSE_PLUGIN_AUTHOR}
    EMAIL ${SKSSE_PLUGIN_EMAIL}
    USE_ADDRESS_LIBRARY
    SOURCES
        src/plugin.cpp
        src/Logger.cpp
        src/PCH.h
        src/Logger.h
        "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/PluginVersion.rc"
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/PCH.h")

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        _UNICODE
)

if(MSVC)
    target_compile_options(${PROJECT_NAME}
        PRIVATE
            /utf-8
            /permissive-
            /Zc:preprocessor
            /wd4200
            /Zi
    )

    target_link_options(${PROJECT_NAME}
        PRIVATE
            "$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
            "$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
    )
endif()

if(DEFINED OUTPUT_FOLDER)
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE")

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
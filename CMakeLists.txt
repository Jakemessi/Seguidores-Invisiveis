# Configuração mínima do CMake
cmake_minimum_required(VERSION 3.21)

# Nome do projeto (será o nome da sua DLL)
project(SeguidoresInvisiveis VERSION 1.0.0 LANGUAGES CXX)

# Verifica se a variável de ambiente do Skyrim existe para definir a pasta secundária
if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER2 "$ENV{SKYRIM_FOLDER}/Data/SKSE/Plugins")
endif()

# ==========================================
# CONFIGURAÇÃO DO PLUGIN
# ==========================================

#   Encontra o CommonLibSSE usando CONFIG
find_package(CommonLibSSE CONFIG REQUIRED)


add_commonlibsse_plugin(${PROJECT_NAME} SOURCES plugin.cpp)

#   LINKA as bibliotecas ao projeto que acabou de ser criado
target_link_libraries(${PROJECT_NAME} PUBLIC CommonLibSSE::CommonLibSSE)

#   Define os padrões adicionais do C++23 e Cabeçalhos Pré-compilados
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23) 
target_precompile_headers(${PROJECT_NAME} PRIVATE PCH.h)



set(OUTPUT_FOLDER "C:/Users/Huehuller/Documents/VSCode Projects/Seguidores Invisiveis/dll")

# Só ativa a automação de cópia se pelo menos a pasta padrão existir
if(DEFINED OUTPUT_FOLDER)
    set(DLL_FOLDER "${OUTPUT_FOLDER}")
    message(STATUS "SKSE plugin output folder 1: ${DLL_FOLDER}")

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

# Copiar para a pasta do Skyrim APENAS se a variável de ambiente realmente existir
if(DEFINED OUTPUT_FOLDER2)
    set(DLL_FOLDER2 "${OUTPUT_FOLDER2}")
    message(STATUS "SKSE plugin output folder 2: ${DLL_FOLDER2}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER2}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER2}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER2}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
            VERBATIM
        )
    endif()
endif()

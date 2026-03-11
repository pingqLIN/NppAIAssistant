cmake_minimum_required(VERSION 3.21)

project(NppAIAssistant LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(NppAIAssistant SHARED
    src/NppAIAssistant.cpp
    src/NppAIAssistantResources.rc
    src/shared/HttpClient.cpp
    src/shared/LLMApiClient.cpp
    src/shared/SecureStorage.cpp
)

target_include_directories(NppAIAssistant PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/notepadpp/PluginsManager
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/notepadpp/DockingWnd
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/scintilla/include
)

target_compile_definitions(NppAIAssistant PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(NppAIAssistant PRIVATE
    comctl32
    crypt32
    shell32
    shlwapi
    winhttp
)

set_target_properties(NppAIAssistant PROPERTIES
    OUTPUT_NAME "NppAIAssistant"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/NppAIAssistant"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/NppAIAssistant"
)

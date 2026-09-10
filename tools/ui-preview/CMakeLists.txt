cmake_minimum_required(VERSION 3.21)
project(NppAIAssistantUiPreview LANGUAGES CXX RC)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
get_filename_component(PLUGIN_ROOT "../.." ABSOLUTE)
option(NPPAI_PREVIEW_ENABLE_NETWORK "Use real HTTP with session-only credentials" OFF)

# Separate preview target: production storage and HTTP are never linked here.
add_library(NppAIAssistant SHARED
    PreviewPlugin.cpp PreviewAdapters.cpp
    "${PLUGIN_ROOT}/src/NppAIAssistantResources.rc"
    "${PLUGIN_ROOT}/src/shared/LLMApiClient.cpp"
    "${PLUGIN_ROOT}/src/shared/PromptGovernance.cpp"
    "${PLUGIN_ROOT}/src/shared/StructuredOutput.cpp")
target_include_directories(NppAIAssistant PRIVATE
    "${PLUGIN_ROOT}/src" "${PLUGIN_ROOT}/src/shared"
    "${PLUGIN_ROOT}/vendor/notepadpp/PluginsManager"
    "${PLUGIN_ROOT}/vendor/notepadpp/DockingWnd"
    "${PLUGIN_ROOT}/vendor/scintilla/include")
target_compile_definitions(NppAIAssistant PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(NppAIAssistant PRIVATE comctl32 shell32 shlwapi)

enable_testing()
add_executable(PreviewIsolationTests PreviewIsolationTests.cpp PreviewAdapters.cpp)
target_include_directories(PreviewIsolationTests PRIVATE "${PLUGIN_ROOT}/src/shared")
target_compile_definitions(PreviewIsolationTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME PreviewIsolationTests COMMAND PreviewIsolationTests)

if(NPPAI_PREVIEW_ENABLE_NETWORK)
    add_executable(LocalGenerationProbe LocalGenerationProbe.cpp
        "${PLUGIN_ROOT}/src/shared/HttpClient.cpp"
        "${PLUGIN_ROOT}/src/shared/LLMApiClient.cpp"
        "${PLUGIN_ROOT}/src/shared/StructuredOutput.cpp")
    target_include_directories(LocalGenerationProbe PRIVATE "${PLUGIN_ROOT}/src/shared")
    target_compile_definitions(LocalGenerationProbe PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    foreach(target NppAIAssistant PreviewIsolationTests)
        target_compile_definitions(${target} PRIVATE NPPAI_PREVIEW_ENABLE_NETWORK)
        target_sources(${target} PRIVATE "${PLUGIN_ROOT}/src/shared/HttpClient.cpp")
        target_link_libraries(${target} PRIVATE crypt32)
    endforeach()
    add_executable(LocalDiscoveryProbe LocalDiscoveryProbe.cpp
        "${PLUGIN_ROOT}/src/shared/HttpClient.cpp"
        "${PLUGIN_ROOT}/src/shared/LLMApiClient.cpp"
        "${PLUGIN_ROOT}/src/shared/StructuredOutput.cpp")
    target_include_directories(LocalDiscoveryProbe PRIVATE "${PLUGIN_ROOT}/src/shared")
    target_compile_definitions(LocalDiscoveryProbe PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

add_executable(EditorWriteIntegrationTests EditorWriteIntegrationTests.cpp PreviewAdapters.cpp
    "${PLUGIN_ROOT}/src/shared/LLMApiClient.cpp"
    "${PLUGIN_ROOT}/src/shared/PromptGovernance.cpp"
    "${PLUGIN_ROOT}/src/shared/StructuredOutput.cpp")
target_include_directories(EditorWriteIntegrationTests PRIVATE
    "${PLUGIN_ROOT}/src" "${PLUGIN_ROOT}/src/shared"
    "${PLUGIN_ROOT}/vendor/notepadpp/PluginsManager"
    "${PLUGIN_ROOT}/vendor/notepadpp/DockingWnd"
    "${PLUGIN_ROOT}/vendor/scintilla/include")
target_compile_definitions(EditorWriteIntegrationTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(EditorWriteIntegrationTests PRIVATE comctl32 shell32 shlwapi)
add_test(NAME EditorWriteIntegrationTests COMMAND EditorWriteIntegrationTests)

cmake_minimum_required(VERSION 3.21)

project(NppAIAssistant LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Portable Notepad++ installs must not depend on a separately installed VC CRT.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(NppAIAssistant SHARED
    src/NppAIAssistant.cpp
    src/NppAIAssistantResources.rc
    src/shared/HttpClient.cpp
    src/shared/LLMApiClient.cpp
    src/shared/PromptGovernance.cpp
    src/shared/StructuredOutput.cpp
    src/shared/SettingsStorage.cpp
    src/shared/SecureStorage.cpp
)

enable_testing()
add_executable(NppAIAssistantCompatibleErrorTests
    tests/CompatibleErrorTests.cpp src/shared/LLMApiClient.cpp src/shared/StructuredOutput.cpp)
target_include_directories(NppAIAssistantCompatibleErrorTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/shared)
target_compile_definitions(NppAIAssistantCompatibleErrorTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME NppAIAssistantCompatibleErrorTests COMMAND NppAIAssistantCompatibleErrorTests)
add_executable(NppAIAssistantPanelInteractionTests tests/PanelInteractionTests.cpp)
target_include_directories(NppAIAssistantPanelInteractionTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shared)
add_test(NAME NppAIAssistantPanelInteractionTests COMMAND NppAIAssistantPanelInteractionTests)
add_executable(NppAIAssistantStructuredOutputTests
    tests/StructuredOutputTests.cpp
    src/shared/StructuredOutput.cpp
)
target_include_directories(NppAIAssistantStructuredOutputTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
)
add_test(NAME NppAIAssistantStructuredOutputTests
         COMMAND NppAIAssistantStructuredOutputTests)

add_executable(NppAIAssistantPromptGovernanceTests
    tests/PromptGovernanceTests.cpp
    src/shared/PromptGovernance.cpp
)
target_include_directories(NppAIAssistantPromptGovernanceTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
)
add_test(NAME NppAIAssistantPromptGovernanceTests
         COMMAND NppAIAssistantPromptGovernanceTests)

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
    advapi32
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

add_executable(NppAIAssistantEditorOutputTests tests/EditorOutputTests.cpp)
target_include_directories(NppAIAssistantEditorOutputTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/shared)
add_test(NAME NppAIAssistantEditorOutputTests COMMAND NppAIAssistantEditorOutputTests)

add_executable(NppAIAssistantWorkbenchFormatTests tests/WorkbenchFormatTests.cpp src/shared/StructuredOutput.cpp)
target_include_directories(NppAIAssistantWorkbenchFormatTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/shared)
add_test(NAME NppAIAssistantWorkbenchFormatTests COMMAND NppAIAssistantWorkbenchFormatTests)

cmake_minimum_required(VERSION 3.24)

project(GitProDiagnosticsSuite
    VERSION 0.1.0
    DESCRIPTION "Master Diagnostics Utility and Auto-Gain Matcher"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GITPRO_ENABLE_WEBVIEW_UI "Enable JUCE WebBrowserComponent dashboard UI" ON)
option(GITPRO_ENABLE_OPENGL_VISUALS "Enable native OpenGL fallback visuals" ON)
option(GITPRO_BUILD_STANDALONE "Build standalone plugin targets" ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
)

FetchContent_MakeAvailable(JUCE)

if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(GITPRO_PLUGIN_FORMATS VST3)

if (GITPRO_BUILD_STANDALONE)
    list(APPEND GITPRO_PLUGIN_FORMATS Standalone)
endif()

if (APPLE)
    list(APPEND GITPRO_PLUGIN_FORMATS AU)
endif()

add_subdirectory(modules/shared_core)
add_subdirectory(modules/shared_ui)
add_subdirectory(plugins/TheProbe)
add_subdirectory(plugins/TheAnalyzer)
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
option(GITPRO_FETCH_WEBVIEW2_SDK "Fetch Microsoft WebView2 SDK for Windows WebView builds" ON)

include(FetchContent)

if (WIN32 AND GITPRO_ENABLE_WEBVIEW_UI AND GITPRO_FETCH_WEBVIEW2_SDK)
    set(GITPRO_WEBVIEW2_VERSION "1.0.1901.177" CACHE STRING "Microsoft.Web.WebView2 NuGet package version")
    set(GITPRO_WEBVIEW2_PACKAGE_PARENT "${CMAKE_BINARY_DIR}/_deps/webview2-nuget")
    set(GITPRO_WEBVIEW2_PACKAGE_DIR "${GITPRO_WEBVIEW2_PACKAGE_PARENT}/Microsoft.Web.WebView2.${GITPRO_WEBVIEW2_VERSION}")
    set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${GITPRO_WEBVIEW2_PACKAGE_PARENT}" CACHE PATH "WebView2 NuGet package parent directory" FORCE)

    FetchContent_Declare(
        MicrosoftWebView2
        URL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${GITPRO_WEBVIEW2_VERSION}"
        SOURCE_DIR "${GITPRO_WEBVIEW2_PACKAGE_DIR}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    FetchContent_MakeAvailable(MicrosoftWebView2)
endif()

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
)

FetchContent_MakeAvailable(JUCE)

list(APPEND CMAKE_MODULE_PATH "${JUCE_SOURCE_DIR}/extras/Build/CMake")

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
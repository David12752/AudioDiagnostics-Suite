add_library(gitpro_shared_ui STATIC
    WebDashboardComponent.cpp
    WebDashboardComponent.h
)

target_include_directories(gitpro_shared_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gitpro_shared_ui
    PUBLIC
        gitpro_shared_core
        juce::juce_gui_extra
)

if (WIN32 AND GITPRO_ENABLE_WEBVIEW_UI)
    find_package(WebView2 REQUIRED)
    target_link_libraries(gitpro_shared_ui PUBLIC juce::juce_webview2)
endif()

if (GITPRO_ENABLE_OPENGL_VISUALS)
    target_link_libraries(gitpro_shared_ui PUBLIC juce::juce_opengl)
    target_compile_definitions(gitpro_shared_ui PUBLIC GITPRO_ENABLE_OPENGL_VISUALS=1)
else()
    target_compile_definitions(gitpro_shared_ui PUBLIC GITPRO_ENABLE_OPENGL_VISUALS=0)
endif()

if (GITPRO_ENABLE_WEBVIEW_UI)
    target_compile_definitions(gitpro_shared_ui PUBLIC GITPRO_ENABLE_WEBVIEW_UI=1 JUCE_WEB_BROWSER=1 JUCE_USE_WIN_WEBVIEW2=1)
else()
    target_compile_definitions(gitpro_shared_ui PUBLIC GITPRO_ENABLE_WEBVIEW_UI=0 JUCE_WEB_BROWSER=0 JUCE_USE_WIN_WEBVIEW2=0)
endif()

if (MSVC)
    target_compile_options(gitpro_shared_ui PRIVATE /FS /EHsc)
endif()

target_compile_definitions(gitpro_shared_ui PUBLIC JUCE_USE_CURL=0)
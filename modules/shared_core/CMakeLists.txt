add_library(gitpro_shared_core STATIC
    SharedCore.cpp
    dsp/AnalysisTypes.h
    dsp/LowBandFftAnalyzer.cpp
    dsp/LowBandFftAnalyzer.h
    ipc/FileDiscoveryRegistry.cpp
    ipc/FileDiscoveryRegistry.h
    ipc/IPCTransport.h
    ipc/LoopbackTransport.h
)

target_include_directories(gitpro_shared_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gitpro_shared_core
    PUBLIC
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
)

target_compile_definitions(gitpro_shared_core
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
)
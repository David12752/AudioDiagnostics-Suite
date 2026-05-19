juce_add_plugin(TheAnalyzer
    COMPANY_NAME "GitPro Audio"
    BUNDLE_ID com.gitproaudio.theanalyzer
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE GPAD
    PLUGIN_CODE GPAN
    FORMATS ${GITPRO_PLUGIN_FORMATS}
    PRODUCT_NAME "The Analyzer")

target_sources(TheAnalyzer
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
)

target_link_libraries(TheAnalyzer
    PRIVATE
        gitpro_shared_core
        gitpro_shared_ui
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(TheAnalyzer
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
)
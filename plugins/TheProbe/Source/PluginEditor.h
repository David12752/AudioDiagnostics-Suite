#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

class TheProbeAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit TheProbeAudioProcessorEditor(TheProbeAudioProcessor& processor);

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    TheProbeAudioProcessor& audioProcessor;
    juce::Label titleLabel;
    juce::Label uuidLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheProbeAudioProcessorEditor)
};
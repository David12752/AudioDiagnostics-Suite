#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "WebDashboardComponent.h"

class TheProbeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit TheProbeAudioProcessorEditor(TheProbeAudioProcessor& processor);
    ~TheProbeAudioProcessorEditor() override;

    void resized() override;

private:
    void timerCallback() override;

    TheProbeAudioProcessor& audioProcessor;
    gitpro::ui::WebDashboardComponent dashboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheProbeAudioProcessorEditor)
};
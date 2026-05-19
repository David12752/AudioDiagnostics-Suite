#include "PluginEditor.h"

TheProbeAudioProcessorEditor::TheProbeAudioProcessorEditor(TheProbeAudioProcessor& processor)
    : AudioProcessorEditor(&processor), audioProcessor(processor)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    addAndMakeVisible(dashboard);
    dashboard.setCommandHandler([this](const juce::String& commandJson)
    {
        JUCE_ASSERT_MESSAGE_THREAD;
        audioProcessor.handleDashboardCommand(commandJson);
        dashboard.publishJsonSnapshot(audioProcessor.createStatusJson());
    });

    dashboard.publishJsonSnapshot(audioProcessor.createStatusJson());
    startTimerHz(20);
    setSize(760, 520);
}

TheProbeAudioProcessorEditor::~TheProbeAudioProcessorEditor()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    stopTimer();
}

void TheProbeAudioProcessorEditor::resized()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    dashboard.setBounds(getLocalBounds());
}

void TheProbeAudioProcessorEditor::timerCallback()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    dashboard.publishJsonSnapshot(audioProcessor.createStatusJson());
}
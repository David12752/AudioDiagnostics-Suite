#include "PluginEditor.h"

TheAnalyzerAudioProcessorEditor::TheAnalyzerAudioProcessorEditor(TheAnalyzerAudioProcessor& processor)
    : AudioProcessorEditor(&processor), audioProcessor(processor)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    addAndMakeVisible(dashboard);
    dashboard.setCommandHandler([this](const juce::String& commandJson)
    {
        JUCE_ASSERT_MESSAGE_THREAD;
        audioProcessor.handleDashboardCommand(commandJson);
        dashboard.publishJsonSnapshot(audioProcessor.createDashboardSnapshotJson());
    });

    dashboard.publishJsonSnapshot(audioProcessor.createDashboardSnapshotJson());
    startTimerHz(60);
    setSize(920, 560);
}

TheAnalyzerAudioProcessorEditor::~TheAnalyzerAudioProcessorEditor()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    stopTimer();
}

void TheAnalyzerAudioProcessorEditor::resized()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    dashboard.setBounds(getLocalBounds());
}

void TheAnalyzerAudioProcessorEditor::timerCallback()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    dashboard.publishJsonSnapshot(audioProcessor.createDashboardSnapshotJson());
}
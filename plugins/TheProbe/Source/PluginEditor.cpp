#include "PluginEditor.h"

TheProbeAudioProcessorEditor::TheProbeAudioProcessorEditor(TheProbeAudioProcessor& processor)
    : AudioProcessorEditor(&processor), audioProcessor(processor)
{
    titleLabel.setText("The Probe", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    uuidLabel.setText("UUID: " + audioProcessor.getInstanceUuid(), juce::dontSendNotification);
    uuidLabel.setJustificationType(juce::Justification::centredLeft);
    uuidLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(uuidLabel);

    setSize(520, 220);
}

void TheProbeAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff111418));
    graphics.setColour(juce::Colour(0xff2b323b));
    graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(12.0f), 8.0f, 1.0f);
}

void TheProbeAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(24);
    titleLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(12);
    uuidLabel.setBounds(bounds.removeFromTop(28));
}
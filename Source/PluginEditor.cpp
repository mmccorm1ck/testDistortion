/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TestDistortionAudioProcessorEditor::TestDistortionAudioProcessorEditor (TestDistortionAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
    lowCutSliderAttachment(audioProcessor.apvts, "LowCut Freq", lowCutSlider),
    highCutSliderAttachment(audioProcessor.apvts, "HighCut Freq", highCutSlider),
    gainInSliderAttachment(audioProcessor.apvts, "Input Gain", gainInSlider),
    gainOutSliderAttachment(audioProcessor.apvts, "Output Gain", gainOutSlider),
    waveshapeFunctionSliderAttachment(audioProcessor.apvts, "Distortion Type", waveshapeFunctionSlider)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.

    for (auto* comp : getComps())
    {
        addAndMakeVisible(comp);
    }

    setSize (600, 400);
}

TestDistortionAudioProcessorEditor::~TestDistortionAudioProcessorEditor()
{
}

//==============================================================================
void TestDistortionAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    //g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void TestDistortionAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();
    auto graphArea = bounds.removeFromTop(bounds.getHeight() * 0.5);
    auto inputArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto outputArea = bounds.removeFromRight(bounds.getWidth() * 0.5);

    gainInSlider.setBounds(inputArea.removeFromTop(inputArea.getHeight() * 0.5));
    gainOutSlider.setBounds(outputArea.removeFromTop(outputArea.getHeight() * 0.5));
    lowCutSlider.setBounds(inputArea);
    highCutSlider.setBounds(outputArea);
    waveshapeFunctionSlider.setBounds(bounds);
}

std::vector<juce::Component*> TestDistortionAudioProcessorEditor::getComps()
{
    return
    {
        &lowCutSlider,
        &highCutSlider,
        &gainInSlider,
        &gainOutSlider,
        &waveshapeFunctionSlider
    };
}
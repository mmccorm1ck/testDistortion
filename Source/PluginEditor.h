/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

float arcTanFunc(float);
float hypTanFunc(float);
float cubicFunc(float);
float pow5Func(float);
float pow7Func(float);
float hardFunc(float);

//==============================================================================
/**
*/

struct CustomRotarySlider : juce::Slider
{
    CustomRotarySlider() : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
        juce::Slider::TextEntryBoxPosition::NoTextBox)
    {

    }
};

class TestDistortionAudioProcessorEditor  : public juce::AudioProcessorEditor,
    juce::AudioProcessorParameter::Listener, juce::Timer
{
public:
    TestDistortionAudioProcessorEditor (TestDistortionAudioProcessor&);
    ~TestDistortionAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {};
    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    TestDistortionAudioProcessor& audioProcessor;

    juce::Atomic<bool> parametersChanged { false };

    CustomRotarySlider lowCutSlider,
        highCutSlider, gainInSlider,
        gainOutSlider, waveshapeFunctionSlider;

    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    Attachment lowCutSliderAttachment,
        highCutSliderAttachment, gainInSliderAttachment,
        gainOutSliderAttachment, waveshapeFunctionSliderAttachment;

    std::vector<juce::Component*> getComps();

    MonoChain monoChain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestDistortionAudioProcessorEditor)
};

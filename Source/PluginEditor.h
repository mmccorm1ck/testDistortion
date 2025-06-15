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

struct TransferGraphComponent : juce::Component, juce::AudioProcessorParameter::Listener, juce::Timer
{
    TransferGraphComponent(TestDistortionAudioProcessor&);
    ~TransferGraphComponent();
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {};
    void timerCallback() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    TestDistortionAudioProcessor& audioProcessor;
    juce::Atomic<bool> parametersChanged{ false };
    MonoChain monoChain;
    juce::Image background;
};

//==============================================================================
/**
*/

struct LookAndFeel : juce::LookAndFeel_V4
{
    void drawRotarySlider(juce::Graphics&,
        int x, int y, int width, int height,
        float sliderPosProportional,
        float rotaryStartAngle,
        float rotartEndAngle,
        juce::Slider& slider) override;
};

struct RotarySliderWithLabels : juce::Slider
{
    RotarySliderWithLabels(juce::RangedAudioParameter& rap, const juce::String& unitSuffix) : juce::Slider(
        juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
        juce::Slider::TextEntryBoxPosition::NoTextBox),
        param(&rap),
        suffix(unitSuffix)
    {
        setLookAndFeel(&lnf);
    }

    ~RotarySliderWithLabels()
    {
        setLookAndFeel(nullptr);
    }

    struct LabelPos
    {
        float pos;
        juce::String label;
    };
    juce::Array<LabelPos> labels;

    void paint(juce::Graphics& g) override;
    juce::Rectangle<int> getSliderBounds() const;
    int getTextHeight() const { return 14; };
    juce::String getDisplayString() const;
private:
    LookAndFeel lnf;
    juce::RangedAudioParameter* param;
    juce::String suffix;
};

class TestDistortionAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    TestDistortionAudioProcessorEditor (TestDistortionAudioProcessor&);
    ~TestDistortionAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    TestDistortionAudioProcessor& audioProcessor;

    RotarySliderWithLabels lowCutSlider,
        highCutSlider, gainInSlider,
        gainOutSlider, waveshapeFunctionSlider;

    TransferGraphComponent transferGraphComponent;

    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    Attachment lowCutSliderAttachment,
        highCutSliderAttachment, gainInSliderAttachment,
        gainOutSliderAttachment, waveshapeFunctionSliderAttachment;

    std::vector<juce::Component*> getComps();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestDistortionAudioProcessorEditor)
};

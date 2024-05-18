/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

enum DistTypes
{
    Soft,
    Hard,
    Tube,
    Tape,
    Rail
};

struct ChainSettings
{
    float lowFreq{ 0 };
    float highFreq{ 0 };
    float inGain{ 0 };
    float outGain{ 0 };
    DistTypes distType { DistTypes::Soft };
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
/**
*/
class TestDistortionAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    TestDistortionAudioProcessor();
    ~TestDistortionAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "Parameters", createParameterLayout()};

private:
    using Filter = juce::dsp::IIR::Filter<float>;
//    using Waveshaper = juce::dsp::WaveShaper<float>;
//    using Gain = juce::dsp::Gain<float>;

    using CutFilter = juce::dsp::ProcessorChain<Filter>;
    using MonoChain = juce::dsp::ProcessorChain<CutFilter, CutFilter>;

    MonoChain leftChain, rightChain;

    enum ChainPossitions
    {
        LowCut,
//        GainIn,
//        WaveShape,
//        GainOut,
        HighCut
    };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestDistortionAudioProcessor)
};

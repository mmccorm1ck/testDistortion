/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <numbers>
#include <cmath>

enum DistTypes
{
    ArcTan,
    HypTan,
    Cubic,
    Pow5,
    Pow7,
    Hard
};

struct ChainSettings
{
    float lowFreq{ 0 };
    float highFreq{ 0 };
    float inGain{ 0 };
    float outGain{ 0 };
    DistTypes distType { DistTypes::ArcTan };
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
    using Waveshaper = juce::dsp::WaveShaper<float>;
    using Gain = juce::dsp::Gain<float>;

    using CutFilter = juce::dsp::ProcessorChain<Filter>;
    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Gain, Waveshaper, Gain, CutFilter>;

    MonoChain leftChain, rightChain;

    enum ChainPositions
    {
        LowCut,
        GainIn,
        WaveShape,
        GainOut,
        HighCut
    };

    using Coefficients = Filter::CoefficientsPtr;
    static void updateCoefficients(Coefficients& old, const Coefficients& replacements);

    template<typename ChainType>
    void updateWaveShape(ChainType& waveshape, const DistTypes& distType)
    {
        switch (distType)
        {
        case ArcTan:
        {
            waveshape.functionToUse = [](float x) {
                return atan(x * std::numbers::pi_v<float> / 2) * 2 / std::numbers::pi_v<float>;
                };
            break;
        }
        case HypTan:
        {
            waveshape.functionToUse = [](float x) {
                return std::tanh(x);
                };
            break;
        }
        case Cubic:
        {
            waveshape.functionToUse = [](float x) {
                float temp;
                if (x >= 1) temp = 2.0 / 3;
                else if (x <= -1) temp = -2.0 / 3;
                else temp = x - (std::pow(x, 3) / 3);
                return temp;
                };
            break;
        }
        case Pow5:
        {
            waveshape.functionToUse = [](float x) {
                float temp;
                if (x >= 1) temp = 11.0 / 15;
                else if (x <= -1) temp = -11.0 / 15;
                else temp = x - (std::pow(x, 3) / 6) - (std::pow(x, 5) / 10);
                return temp;
                };

            break;
        }
        case Pow7:
        {
            waveshape.functionToUse = [](float x) {
                float temp;
                if (x >= 1) temp = 19.0 / 24;
                else if (x <= -1) temp = -19.0 / 24;
                else temp = x - (std::pow(x, 3) / 12) - (std::pow(x, 5) / 16) - (std::pow(x, 7) / 16);
                return temp;
                };
            break;
        }
        case Hard:
        {
            waveshape.functionToUse = [](float x) {
                float temp = x;
                if (temp >= 1) temp = 1.0;
                else if (temp <= -1) temp = -1.0;
                return (temp);
                };
            break;
        }
        }
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestDistortionAudioProcessor)
};

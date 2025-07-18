/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <numbers>
#include <cmath>

//==============================================================================
TestDistortionAudioProcessor::TestDistortionAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

TestDistortionAudioProcessor::~TestDistortionAudioProcessor()
{
}

//==============================================================================
const juce::String TestDistortionAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TestDistortionAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TestDistortionAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TestDistortionAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double TestDistortionAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TestDistortionAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int TestDistortionAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TestDistortionAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String TestDistortionAudioProcessor::getProgramName (int index)
{
    return {};
}

void TestDistortionAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void TestDistortionAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    juce::dsp::ProcessSpec spec;

    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    updateChain();

    leftChannelFifo.prepare(samplesPerBlock);
    rightChannelFifo.prepare(samplesPerBlock);
}

void TestDistortionAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TestDistortionAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void TestDistortionAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    updateChain();

    juce::dsp::AudioBlock<float> block(buffer);

    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    leftChain.process(leftContext);
    rightChain.process(rightContext);

    juce::AudioBuffer<float> fifoBuffer(buffer.getNumChannels(), buffer.getNumSamples());
    fifoBuffer.copyFrom(0, 0, leftChain.get<ChainPositions::FifoBlk>().getBuffer().getReadPointer(0), leftChain.get<ChainPositions::FifoBlk>().getNumSamples());
    fifoBuffer.copyFrom(1, 0, rightChain.get<ChainPositions::FifoBlk>().getBuffer().getReadPointer(0), rightChain.get<ChainPositions::FifoBlk>().getNumSamples());
    leftChannelFifo.update(fifoBuffer);
    rightChannelFifo.update(fifoBuffer);
}

//==============================================================================
bool TestDistortionAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* TestDistortionAudioProcessor::createEditor()
{
    return new TestDistortionAudioProcessorEditor (*this);
//    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void TestDistortionAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void TestDistortionAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
        updateChain();
    }
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;

    settings.lowFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    settings.highFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    settings.inGain = apvts.getRawParameterValue("Input Gain")->load();
    settings.outGain = apvts.getRawParameterValue("Output Gain")->load();
    settings.distType = static_cast<DistTypes>(apvts.getRawParameterValue("Distortion Type")->load());

    settings.lowCutBypassed = apvts.getRawParameterValue("LowCut Bypassed")->load() > 0.5f;
    settings.highCutBypassed = apvts.getRawParameterValue("HighCut Bypassed")->load() > 0.5f;
    settings.distortionBypassed = apvts.getRawParameterValue("Distortion Bypassed")->load() > 0.5f;

    return settings;
}

void TestDistortionAudioProcessor::updateCoefficients(Coefficients& old, const Coefficients& replacements)
{
    *old = *replacements;
}

void TestDistortionAudioProcessor::updateLowCut(const ChainSettings& chainSettings)
{
    auto lowCutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowFreq, getSampleRate(), 1);
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();

    leftChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    rightChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);

    updateCoefficients(leftLowCut.get<0>().coefficients, lowCutCoefficients[0]);
    updateCoefficients(rightLowCut.get<0>().coefficients, lowCutCoefficients[0]);
}

void TestDistortionAudioProcessor::updateHighCut(const ChainSettings& chainSettings)
{
    auto highCutCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highFreq, getSampleRate(), 1);
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();

    leftChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    rightChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);

    updateCoefficients(leftHighCut.get<0>().coefficients, highCutCoefficients[0]);
    updateCoefficients(rightHighCut.get<0>().coefficients, highCutCoefficients[0]);
}

void TestDistortionAudioProcessor::updateGain(const ChainSettings& chainSettings)
{
    leftChain.setBypassed<ChainPositions::GainIn>(chainSettings.distortionBypassed);
    rightChain.setBypassed<ChainPositions::GainIn>(chainSettings.distortionBypassed);
    leftChain.setBypassed<ChainPositions::GainOut>(chainSettings.distortionBypassed);
    rightChain.setBypassed<ChainPositions::GainOut>(chainSettings.distortionBypassed);

    leftChain.get<ChainPositions::GainIn>().setGainDecibels(chainSettings.inGain);
    rightChain.get<ChainPositions::GainIn>().setGainDecibels(chainSettings.inGain);
    leftChain.get<ChainPositions::GainOut>().setGainDecibels(chainSettings.outGain);
    rightChain.get<ChainPositions::GainOut>().setGainDecibels(chainSettings.outGain);
}

void TestDistortionAudioProcessor::updateWaveShaper(const ChainSettings& chainSettings)
{
    auto& waveshapeLeft = leftChain.get<ChainPositions::WaveShape>();
    auto& waveshapeRight = rightChain.get<ChainPositions::WaveShape>();

    leftChain.setBypassed<ChainPositions::WaveShape>(chainSettings.distortionBypassed);
    rightChain.setBypassed<ChainPositions::WaveShape>(chainSettings.distortionBypassed);

    updateWaveShape(waveshapeLeft, chainSettings.distType);
    updateWaveShape(waveshapeRight, chainSettings.distType);
}

void TestDistortionAudioProcessor::updateChain()
{
    auto chainSettings = getChainSettings(apvts);
    updateHighCut(chainSettings);
    updateLowCut(chainSettings);
    updateGain(chainSettings);
    updateWaveShaper(chainSettings);
}

juce::AudioProcessorValueTreeState::ParameterLayout TestDistortionAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", "LowCut Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq", "HighCut Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20000.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Input Gain", "Input Gain", juce::NormalisableRange<float>(-25.0f, 25.0f, 0.5f, 1.f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Output Gain", "Output Gain", juce::NormalisableRange<float>(-25.0f, 25.0f, 0.5f, 1.f), 0.0f));

    juce::StringArray stringArray;
    stringArray.add("ArcTan");
    stringArray.add("HypTan");
    stringArray.add("Cubic");
    stringArray.add("Pow5");
    stringArray.add("Pow7");
    stringArray.add("Hard");

    layout.add(std::make_unique<juce::AudioParameterChoice>("Distortion Type", "Distortion Type", stringArray, 0));

    layout.add(std::make_unique<juce::AudioParameterBool>("LowCut Bypassed", "LowCut Bypassed", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("HighCut Bypassed", "HighCut Bypassed", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Distortion Bypassed", "Distortion Bypassed", false));

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestDistortionAudioProcessor();
}

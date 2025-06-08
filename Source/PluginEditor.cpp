/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

TransferGraphComponent::TransferGraphComponent(TestDistortionAudioProcessor& p) : audioProcessor(p)
{
    const auto& params = audioProcessor.getParameters();
    for (auto param : params)
    {
        param->addListener(this);
    }

    startTimerHz(60);
}

TransferGraphComponent::~TransferGraphComponent()
{
    const auto& params = audioProcessor.getParameters();
    for (auto param : params)
    {
        param->removeListener(this);
    }
}

void TransferGraphComponent::parameterValueChanged(int parameterIndex, float newValue)
{
    parametersChanged.set(true);
}

void TransferGraphComponent::timerCallback()
{
    if (parametersChanged.compareAndSetBool(false, true))
    {
        auto chainSettings = getChainSettings(audioProcessor.apvts);
        repaint();
    }
}

void TransferGraphComponent::paint(juce::Graphics& g)
{
    using namespace juce;
    g.fillAll(Colours::black);

    auto graphArea = getLocalBounds();
    auto w = graphArea.getWidth();

    auto& waveShape = monoChain.get<ChainPositions::WaveShape>();
    DistTypes distType = static_cast<DistTypes>(audioProcessor.apvts.getRawParameterValue("Distortion Type")->load());

    float (*wsFunc)(float);
    wsFunc = arcTanFunc;
    switch (distType)
    {
    case ArcTan:
    {
        wsFunc = arcTanFunc;
        break;
    }
    case HypTan:
    {
        wsFunc = hypTanFunc;
        break;
    }
    case Cubic:
    {
        wsFunc = cubicFunc;
        break;
    }
    case Pow5:
    {
        wsFunc = pow5Func;
        break;
    }
    case Pow7:
    {
        wsFunc = pow7Func;
        break;
    }
    default:
    {
        wsFunc = hardFunc;
        break;
    }
    }

    auto sampleRate = audioProcessor.getSampleRate();

    std::vector<double> mags;
    mags.resize(w);

    for (int i = 0; i < w; ++i) {
        float input = 2 * static_cast<float>(i) / w;
        mags[i] = wsFunc(input);
    }

    Path functionPath;

    const double outputMin = graphArea.getBottom();
    const double outputMax = graphArea.getY();
    auto map = [outputMin, outputMax](double input)
        {
            return jmap(input, 0.0, 1.0, outputMin, outputMax);
        };
    functionPath.startNewSubPath(graphArea.getX(), map(mags.front()));

    for (size_t i = 1; i < mags.size(); ++i) {
        functionPath.lineTo(graphArea.getX() + i, map(mags[i]));
    }

    g.setColour(Colours::blue);
    g.drawRoundedRectangle(graphArea.toFloat(), 4.f, 1.f);
    g.setColour(Colours::white);
    g.strokePath(functionPath, PathStrokeType(2.f));
}

//==============================================================================
TestDistortionAudioProcessorEditor::TestDistortionAudioProcessorEditor (TestDistortionAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
    lowCutSlider(*audioProcessor.apvts.getParameter("LowCut Freq"), "Hz"),
    highCutSlider(*audioProcessor.apvts.getParameter("HighCut Freq"), "Hz"),
    gainInSlider(*audioProcessor.apvts.getParameter("Input Gain"), "dB"),
    gainOutSlider(*audioProcessor.apvts.getParameter("Output Gain"), "dB"),
    waveshapeFunctionSlider(*audioProcessor.apvts.getParameter("Distortion Type"), ""),
    transferGraphComponent(audioProcessor),
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

    setSize (600, 500);
}

TestDistortionAudioProcessorEditor::~TestDistortionAudioProcessorEditor()
{

}

//==============================================================================
void TestDistortionAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (Colours::black);
}

void TestDistortionAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();
    auto graphArea = bounds.removeFromTop(bounds.getHeight() * 0.6);
    auto inputArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto outputArea = bounds.removeFromRight(bounds.getWidth() * 0.5);

    transferGraphComponent.setBounds(graphArea);
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
        &waveshapeFunctionSlider,
        &transferGraphComponent
    };
}

float arcTanFunc(float x)
{
    return atan(x * std::numbers::pi_v<float> / 2) * 2 / std::numbers::pi_v<float>;
}

float hypTanFunc(float x)
{
    return std::tanh(x);
}

float cubicFunc(float x)
{
    float temp;
    if (x >= 1) temp = 2.f / 3;
    else if (x <= -1) temp = -2.f / 3;
    else temp = x - (std::pow(x, 3) / 3);
    return temp;
}

float pow5Func(float x)
{
    float temp;
    if (x >= 1) temp = 11.f / 15;
    else if (x <= -1) temp = -11.f / 15;
    else temp = x - (std::pow(x, 3) / 6) - (std::pow(x, 5) / 10);
    return temp;
}

float pow7Func(float x)
{
    float temp;
    if (x >= 1) temp = 19.f / 24;
    else if (x <= -1) temp = -19.f / 24;
    else temp = x - (std::pow(x, 3) / 12) - (std::pow(x, 5) / 16) - (std::pow(x, 7) / 16);
    return temp;
}

float hardFunc(float x)
{
    float temp = x;
    if (temp >= 1) temp = 1.f;
    else if (temp <= -1) temp = -1.f;
    return (temp);
}
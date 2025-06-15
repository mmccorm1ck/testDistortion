/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

void LookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotartEndAngle,
    juce::Slider& slider)
{
    using namespace juce;

    auto bounds = Rectangle<float>(x, y, width, height);

    g.setColour(Colours::grey);
    g.fillEllipse(bounds);
    g.setColour(Colours::blue);
    g.drawEllipse(bounds, 1.f);

    if (auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        auto centre = bounds.getCentre();
        Path p;

        Rectangle<float> r;
        r.setLeft(centre.getX() - 2);
        r.setRight(centre.getX() + 2);
        r.setTop(bounds.getY());
        r.setBottom(bounds.getY() + 16);

        p.addRoundedRectangle(r, 2.f);

        jassert(rotaryStartAngle < rotartEndAngle);
        auto sliderAngRad = jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotartEndAngle);

        p.applyTransform(AffineTransform().rotated(sliderAngRad, centre.getX(), centre.getY()));

        g.fillPath(p);

        g.setFont(rswl->getTextHeight());
        auto text = rswl->getDisplayString();
        auto strWidth = g.getCurrentFont().getStringWidth(text);

        r.setSize(strWidth + 4, rswl->getTextHeight() + 4);
        r.setCentre(centre);

        g.setColour(Colours::black);
        g.fillRect(r);

        g.setColour(Colours::white);
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

void RotarySliderWithLabels::paint(juce::Graphics& g)
{
    using namespace juce;

    auto startAng = degreesToRadians(225.f);
    auto endAng = degreesToRadians(135.f) + MathConstants<float>::twoPi;

    auto range = getRange();
    auto sliderBounds = getSliderBounds();

    getLookAndFeel().drawRotarySlider(g, sliderBounds.getX(), sliderBounds.getY(),
        sliderBounds.getWidth(), sliderBounds.getHeight(),
        jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
        startAng, endAng, *this);

    auto centre = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5f;

    g.setColour(Colours::white);
    g.setFont(getTextHeight());

    auto numChoices = labels.size();
    for (int i = 0; i < numChoices; ++i)
    {
        auto pos = labels[i].pos;
        jassert(0.f <= pos);
        jassert(pos <= 1.f);

        auto ang = jmap(pos, 0.f, 1.f, startAng, endAng);
        auto drawPoint = centre.getPointOnCircumference(radius + getTextHeight(), ang);
        Rectangle<float> r;
        auto str = labels[i].label;
        r.setSize(g.getCurrentFont().getStringWidth(str), getTextHeight());
        r.setCentre(drawPoint);
        r.setY(r.getY() + getTextHeight() * 0.5f);

        g.drawFittedText(str, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
    {
        return choiceParam->getCurrentChoiceName();
    }

    juce::String str;
    bool addK = false;
    
    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        float val = getValue();

        if (val > 999.f)
        {
            val /= 1000.f;
            addK = true;
        }
        str = juce::String(val, (addK ? 2 : 0));
    }
    else
    {
        jassertfalse;
    }

    if (suffix.isNotEmpty())
    {
        str << " ";
        if (addK)
        {
            str << "k";
        }
        str << suffix;
    }
    return str;
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    auto bounds = getLocalBounds();
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight()) - (getTextHeight() * 2);

    juce::Rectangle<int> r;
    r.setSize(size, size);
    r.setCentre(bounds.getCentreX(), 0);
    r.setY(2);
    return r;
}

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
    
    g.drawImage(background, getLocalBounds().toFloat());

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

void TransferGraphComponent::resized()
{
    using namespace juce;
    background = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g(background);

    Array<float> xAxis
    {
        0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1,
        1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9
    };
    g.setColour(Colours::grey);
    float dashPattern[2];
    dashPattern[0] = 5.f;
    dashPattern[1] = 5.f;
    Line<float> l;
    for (auto a : xAxis)
    {
        auto x = jmap(a, 0.f, 2.f, 0.f, float(getWidth()));
        l.setStart(x, 0.f);
        l.setEnd(x, getHeight());
        if (a == 1.f)
            g.drawDashedLine(l, dashPattern, 2, 2.f);
        else
            g.drawDashedLine(l, dashPattern, 2, 1.f);
    }
    Array<float> yAxis
    {
        0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9
    };
    for (auto a : yAxis)
    {
        auto y = jmap(a, 0.f, 1.f, 0.f, float(getHeight()));
        l.setStart(0.f, y);
        l.setEnd(getWidth(), y);
        g.drawDashedLine(l, dashPattern, 2, 1.f);
    }
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

    lowCutSlider.labels.add({ 0.f, "20Hz" });
    lowCutSlider.labels.add({ 1.f, "20kHz" });
    highCutSlider.labels.add({ 0.f, "20Hz" });
    highCutSlider.labels.add({ 1.f, "20kHz" });
    gainInSlider.labels.add({ 0.f, "0dB" });
    gainInSlider.labels.add({ 1.f, "50dB" });
    gainOutSlider.labels.add({ 0.f, "-50dB" });
    gainOutSlider.labels.add({ 1.f, "0dB" });

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
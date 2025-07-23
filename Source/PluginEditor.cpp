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

void LookAndFeel::drawToggleButton(juce::Graphics& g,
    juce::ToggleButton& toggleButton,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    using namespace juce;

    Path iconButton;

    auto bounds = toggleButton.getLocalBounds();
    auto size = jmin(bounds.getWidth(), bounds.getHeight()) - 2;
    auto r = bounds.withSizeKeepingCentre(size, size).toFloat();
    bool toggled = toggleButton.getToggleState();
    auto colour = Colours::limegreen;

    if (toggled)
    {
        iconButton.startNewSubPath(r.getBottomLeft());
        iconButton.lineTo(r.getTopRight());
        iconButton.startNewSubPath(r.getBottomRight());
        iconButton.lineTo(r.getTopLeft());
        colour = Colours::darkred;
    }
    else
    {
        iconButton.addCentredArc(r.getCentreX(), r.getCentreY(),
            size * 0.5, size * 0.5, 0.f, 0.f, degreesToRadians(360.f), true);
    }

    PathStrokeType pst(2.f, PathStrokeType::JointStyle::curved);

    g.setColour(colour);
    g.strokePath(iconButton, pst);
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
        juce::String str = labels[i].label;
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
        if (suffix == "dB" && getValue() > 0)
            str = "+" + str;
        str << " ";
        if (addK)
            str << "k";
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
    if (labels.size() == 0)
        r.setCentre(bounds.getCentre());
    else
    {
        r.setCentre(bounds.getCentreX(), 0);
        r.setY(2);
    }
    return r;
}

TransferGraphComponent::TransferGraphComponent(TestDistortionAudioProcessor& p) :
    audioProcessor(p),
    leftChannelFifo(&audioProcessor.leftChannelFifo),
    rightChannelFifo(&audioProcessor.rightChannelFifo)
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
    juce::AudioBuffer<float> tempIncomingBuffer;
    float prevMagnitude = maxMagnitude;
    maxMagnitude = 0.0;

    while (leftChannelFifo->getNumCompleteBuffersAvailable() > 0)
    {
        if (leftChannelFifo->getAudioBuffer(tempIncomingBuffer))
        {
            auto size = tempIncomingBuffer.getNumSamples();
            float incomingMagnitude = tempIncomingBuffer.getMagnitude(0, size);
            if (incomingMagnitude > maxMagnitude)
            {
                maxMagnitude = incomingMagnitude;
            }
        }
    }

    while (rightChannelFifo->getNumCompleteBuffersAvailable() > 0)
    {
        if (rightChannelFifo->getAudioBuffer(tempIncomingBuffer))
        {
            auto size = tempIncomingBuffer.getNumSamples();
            float incomingMagnitude = tempIncomingBuffer.getMagnitude(0, size);
            if (incomingMagnitude > maxMagnitude)
            {
                maxMagnitude = incomingMagnitude;
            }
        }
    }
    dampedMagnitude = (maxMagnitude * 3 + prevMagnitude) / 4;

    if (parametersChanged.compareAndSetBool(false, true))
    {
        updateChain();
    }

    repaint();
}

void TransferGraphComponent::updateChain()
{
    auto chainSettings = getChainSettings(audioProcessor.apvts);

    monoChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    monoChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    monoChain.setBypassed<ChainPositions::WaveShape>(chainSettings.distortionBypassed);
}

void TransferGraphComponent::paint(juce::Graphics& g)
{
    using namespace juce;
    g.fillAll(Colours::black);

    auto graphArea = getLocalBounds();

    double aspectRatio = static_cast<double>(getWidth()) / getHeight();

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
        float input = aspectRatio * static_cast<float>(i) / w;
        mags[i] = wsFunc(input);
    }

    Path functionPath;

    const double inputMax = graphArea.getRight();
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

    int magX = jmap(static_cast<double>(dampedMagnitude), 0.0, aspectRatio, 0.0, inputMax);
    int magY = map(static_cast<double>(wsFunc(dampedMagnitude)));

    bool distBypassed = monoChain.isBypassed<ChainPositions::WaveShape>();

    g.setColour(Colours::lightblue);
    if (distBypassed)
    {
        g.drawVerticalLine(magX, outputMax, outputMin);
    }
    else
    {
        g.drawHorizontalLine(magY, 0.0, magX);
        g.drawVerticalLine(magX, magY, outputMin);
    }
    g.setColour(Colours::blue);
    g.drawRoundedRectangle(graphArea.toFloat(), 4.f, 1.f);
    if (distBypassed) g.setColour(Colours::grey);
    else g.setColour(Colours::white);
    g.strokePath(functionPath, PathStrokeType(2.f));
}

void TransferGraphComponent::resized()
{
    using namespace juce;
    background = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g(background);

    float aspectRatio = static_cast<float>(getWidth()) / getHeight();

    Array<float> xAxis;
    for (int i = 0; i < aspectRatio; i++)
    {
        for (int j = 1; j <= 10; j++)
        {
            float temp = i + static_cast<float>(j) / 10;
            if (temp >= aspectRatio) break;
            xAxis.add(temp);
        }
    }
    g.setColour(Colours::grey);
    float dashPattern[2];
    dashPattern[0] = 5.f;
    dashPattern[1] = 5.f;
    Line<float> l;
    for (auto a : xAxis)
    {
        auto x = jmap(a, 0.f, aspectRatio, 0.f, float(getWidth()));
        l.setStart(x, 0.f);
        l.setEnd(x, getHeight());
        if (fmod(a,1) == 0.f)
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
    waveshapeFunctionSliderAttachment(audioProcessor.apvts, "Distortion Type", waveshapeFunctionSlider),
    lowCutBypassButtonAttachment(audioProcessor.apvts, "LowCut Bypassed", lowCutBypassButton),
    highCutBypassButtonAttachment(audioProcessor.apvts, "HighCut Bypassed", highCutBypassButton),
    distortionBypassButtonAttachment(audioProcessor.apvts, "Distortion Bypassed", distortionBypassButton)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.

    lowCutSlider.labels.add({ 0.f, "20Hz" });
    lowCutSlider.labels.add({ 1.f, "20kHz" });
    highCutSlider.labels.add({ 0.f, "20Hz" });
    highCutSlider.labels.add({ 1.f, "20kHz" });
    gainInSlider.labels.add({ 0.f, "-25dB" });
    gainInSlider.labels.add({ 1.f, "+25dB" });
    gainOutSlider.labels.add({ 0.f, "-25dB" });
    gainOutSlider.labels.add({ 1.f, "+25dB" });

    for (auto* comp : getComps())
    {
        addAndMakeVisible(comp);
    }

    lowCutBypassButton.setLookAndFeel(&lnf);
    highCutBypassButton.setLookAndFeel(&lnf);
    distortionBypassButton.setLookAndFeel(&lnf);

    setSize (600, 400);
}

TestDistortionAudioProcessorEditor::~TestDistortionAudioProcessorEditor()
{
    lowCutBypassButton.setLookAndFeel(nullptr);
    highCutBypassButton.setLookAndFeel(nullptr);
    distortionBypassButton.setLookAndFeel(nullptr);
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
    int controlAreaHeigh = bounds.getWidth() / 3;
    auto graphArea = bounds.removeFromTop(bounds.getHeight() - controlAreaHeigh);
    auto inputArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto outputArea = bounds.removeFromRight(bounds.getWidth() * 0.5);

    transferGraphComponent.setBounds(graphArea);
    float sliderHeight = inputArea.getHeight() * 0.45;
    gainInSlider.setBounds(inputArea.removeFromTop(sliderHeight));
    gainOutSlider.setBounds(outputArea.removeFromTop(sliderHeight));
    lowCutSlider.setBounds(inputArea.removeFromBottom(sliderHeight));
    highCutSlider.setBounds(outputArea.removeFromBottom(sliderHeight));
    lowCutBypassButton.setBounds(inputArea);
    highCutBypassButton.setBounds(outputArea);
    distortionBypassButton.setBounds(bounds.removeFromTop(inputArea.getHeight()));
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
        &transferGraphComponent,
        &lowCutBypassButton,
        &highCutBypassButton,
        &distortionBypassButton
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
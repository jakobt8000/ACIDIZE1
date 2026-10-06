#pragma once

#include "PluginProcessor.h"

namespace acid
{
    namespace colours
    {
        const juce::Colour ink       { 0xff1A1A1A };
        const juce::Colour light     { 0xffFFFDF0 };   // light yellow (was white)
        const juce::Colour panel     { 0xffFFF7CF };   // yellow panels (was grey)
        const juce::Colour line      { 0xffF2E8B6 };
        const juce::Colour grid      { 0xffE2D592 };
        const juce::Colour blackKey  { 0xffFFF4C2 };
        const juce::Colour outScale  { 0xffEFE4B4 };
        const juce::Colour noteFill  { 0xffEBDD93 };
        const juce::Colour muted     { 0xff6E6A55 };
        const juce::Colour muted2    { 0xff9A9374 };
        const juce::Colour faint     { 0xffC9BD8C };
        const juce::Colour grey      { 0xff8A8A8A };
    }

    juce::Font monoFont (float height, float kerning = 0.12f);
}

//==============================================================================
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

//==============================================================================
// A round outlined button, used for PLAY, i and the arrows.
class RoundButton : public juce::Button
{
public:
    enum class Kind { play, info, arrowLeft, arrowRight };
    RoundButton (Kind k) : juce::Button ({}), kind (k) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
private:
    Kind kind;
};

//==============================================================================
class BarsDisplay : public juce::Component, private juce::Timer
{
public:
    explicit BarsDisplay (AcidizeProcessor&);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    AcidizeProcessor& proc;
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, numBars = 64;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2> smoothed {};
    std::array<float, numBars> levels {};
};

//==============================================================================
class PianoRoll : public juce::Component, private juce::Timer
{
public:
    explicit PianoRoll (AcidizeProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    static constexpr int rowH = 14, numRows = 25, keyW = 42;
    static constexpr int rollH = rowH * numRows;   // 350
    static constexpr int totalH = 428;

private:
    void timerCallback() override;
    float colW() const { return (float) (getWidth() - keyW) / (float) acid::numSteps; }
    int pitchAtY (float y) const;
    juce::Rectangle<float> accentRect (int col) const;
    juce::Rectangle<float> slideRect (int col) const;
    void stepParam (const juce::String& id, int delta);

    AcidizeProcessor& proc;
    int dragStep = -1;
    bool dragHit = false, dragMoved = false;
    int lastPlay = -2, lastRoot = -1, lastScale = -1;

    juce::Rectangle<int> rootPrev, rootNext, scalePrev, scaleNext;
};

//==============================================================================
class InfoPanel : public juce::Component
{
public:
    InfoPanel();
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::Image logo;
    juce::Rectangle<int> closeArea { 990 - 46, 6, 40, 40 };
};

//==============================================================================
// The full-size UI (designed at 1360 px wide). The editor shows it scaled down.
class AcidizeView : public juce::Component, private juce::Timer
{
public:
    explicit AcidizeView (AcidizeProcessor&);
    ~AcidizeView() override;

    std::function<void()> onSizeChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    static constexpr int width = 1360, topH = 232, barH = 28;

private:
    void timerCallback() override;
    void setRollOpen (bool open);
    void openInfo();
    juce::String centreText() const;

    AcidizeProcessor& proc;
    KnobLookAndFeel knobLnf;

    struct Knob
    {
        juce::String id, label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    std::vector<std::unique_ptr<Knob>> knobs;   // [0] = cutoff, [1..10] = grid, [11] = output
    Knob* addKnob (const juce::String& id, const juce::String& label);

    RoundButton playButton { RoundButton::Kind::play };
    RoundButton infoButton { RoundButton::Kind::info };
    RoundButton prevButton { RoundButton::Kind::arrowLeft };
    RoundButton nextButton { RoundButton::Kind::arrowRight };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playAttachment;

    BarsDisplay bars;
    PianoRoll roll;

    juce::Rectangle<int> navTextArea, rollToggleArea, rndArea;
    juce::String lastCentre;
};

//==============================================================================
// Window shown inside Ableton: the design scaled to 75 %.
class AcidizeEditor : public juce::AudioProcessorEditor
{
public:
    explicit AcidizeEditor (AcidizeProcessor&);
    static constexpr float scale = 0.75f;
private:
    void updateSize();
    AcidizeView view;
};

// Info window content, scaled the same way as the editor.
class InfoWindowContent : public juce::Component
{
public:
    InfoWindowContent();
private:
    InfoPanel panel;
};

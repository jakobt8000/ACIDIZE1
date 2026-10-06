#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

using namespace acid;

//==============================================================================
juce::Font acid::monoFont (float cssPx, float kerning)
{
    static juce::Typeface::Ptr plex = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoRegular_ttf,
                                                                               (size_t) BinaryData::IBMPlexMonoRegular_ttfSize);
    return juce::Font (juce::FontOptions (plex).withPointHeight (cssPx).withKerningFactor (kerning));
}

static void drawLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float cssPx,
                       juce::Colour colour, float kerning = 0.12f, juce::Justification just = juce::Justification::centred)
{
    g.setColour (colour);
    g.setFont (monoFont (cssPx, kerning));
    g.drawText (text, area, just, false);
}

static void drawChevron (juce::Graphics& g, juce::Rectangle<float> box, bool pointsLeft, float stroke)
{
    juce::Path p;
    const float w = box.getWidth(), h = box.getHeight();
    if (pointsLeft) { p.startNewSubPath (box.getX() + w * 0.75f, box.getY() + h * 0.11f); p.lineTo (box.getX() + w * 0.25f, box.getCentreY()); p.lineTo (box.getX() + w * 0.75f, box.getBottom() - h * 0.11f); }
    else            { p.startNewSubPath (box.getX() + w * 0.25f, box.getY() + h * 0.11f); p.lineTo (box.getX() + w * 0.75f, box.getCentreY()); p.lineTo (box.getX() + w * 0.25f, box.getBottom() - h * 0.11f); }
    g.strokePath (p, juce::PathStrokeType (stroke));
}

//==============================================================================
void KnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    const float size = (float) juce::jmin (w, h);
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.5f - 1.0f;

    g.setColour (colours::light);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (colours::ink);
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.2f);

    const float a = startAngle + pos * (endAngle - startAngle);
    const float s = std::sin (a), co = std::cos (a);
    juce::Path needle;
    needle.startNewSubPath (c.x + r * 0.06f * s, c.y - r * 0.06f * co);
    needle.lineTo (c.x + r * 0.62f * s, c.y - r * 0.62f * co);
    g.strokePath (needle, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
void RoundButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto b = getLocalBounds().toFloat();

    if (kind == Kind::arrowLeft || kind == Kind::arrowRight)
    {
        g.setColour (colours::ink.withAlpha (down ? 0.6f : 1.0f));
        const bool left = kind == Kind::arrowLeft;
        auto box = juce::Rectangle<float> (12.0f, 18.0f).withCentre (b.getCentre());
        box.setX (left ? b.getX() : b.getRight() - 12.0f);
        drawChevron (g, box, left, 2.0f);
        return;
    }

    const bool on = getToggleState();
    auto circle = b.reduced (0.6f);
    g.setColour (on ? colours::ink : colours::light);
    g.fillEllipse (circle);
    g.setColour (colours::ink);
    g.drawEllipse (circle, 1.2f);
    if (highlighted && ! on) { g.setColour (colours::ink.withAlpha (0.04f)); g.fillEllipse (circle); }

    const auto c = b.getCentre();
    if (kind == Kind::play)
    {
        g.setColour (on ? colours::light : colours::ink);
        if (on)
            g.fillRect (juce::Rectangle<float> (10.0f, 10.0f).withCentre (c));
        else
        {
            juce::Path tri;
            tri.addTriangle (c.x - 5.0f, c.y - 6.0f, c.x + 6.0f, c.y, c.x - 5.0f, c.y + 6.0f);
            g.fillPath (tri);
        }
    }
    else
    {
        drawLabel (g, "i", b, 18.0f, colours::ink, 0.0f);
    }
}

//==============================================================================
BarsDisplay::BarsDisplay (AcidizeProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void BarsDisplay::timerCallback()
{
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    proc.copyRecentOutput (fftData.data(), fftSize);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    for (int b = 0; b < fftSize / 2; ++b)
    {
        const float mag = fftData[(size_t) b] / (float) fftSize;   // same scale as the Web Audio analyser in the mockup
        smoothed[(size_t) b] = 0.55f * smoothed[(size_t) b] + 0.45f * mag;
    }

    const double sampleRate = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    for (int i = 0; i < numBars; ++i)
    {
        const double f = 30.0 * std::pow (600.0, (double) i / (numBars - 1));
        const int bin = juce::jlimit (1, fftSize / 2 - 1, (int) std::round (f / (sampleRate / fftSize)));
        const float db = 20.0f * std::log10 (juce::jmax (smoothed[(size_t) bin], 1.0e-9f));
        levels[(size_t) i] = juce::jlimit (0.0f, 1.0f, (db + 95.0f) / 75.0f);
    }
    repaint();
}

void BarsDisplay::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();
    g.setColour (colours::ink);
    g.fillRect (0.0f, H - 1.5f, W, 1.0f);

    const float step = W / (float) numBars;
    for (int i = 0; i < numBars; ++i)
    {
        const float h = juce::jmax (2.0f, levels[(size_t) i] * (H - 4.0f));
        g.fillRect (i * step + (step - 3.0f) * 0.5f, H - 1.0f - h, 3.0f, h);
    }
}

//==============================================================================
PianoRoll::PianoRoll (AcidizeProcessor& p) : proc (p)
{
    rootPrev  = { 78, 389, 32, 32 };  rootNext  = { 144, 389, 32, 32 };
    scalePrev = { 212, 389, 32, 32 }; scaleNext = { 374, 389, 32, 32 };
    startTimerHz (30);
}

void PianoRoll::timerCallback()
{
    const int play  = proc.playStep.load();
    const int root  = (int) proc.apvts.getRawParameterValue ("root")->load();
    const int scale = (int) proc.apvts.getRawParameterValue ("scale")->load();

    if (root != lastRoot || scale != lastScale)
    {
        if (lastRoot >= 0) proc.snapPatternToScale();
        lastRoot = root; lastScale = scale;
        repaint();
    }
    if (play != lastPlay) { lastPlay = play; repaint(); }
}

int PianoRoll::pitchAtY (float y) const
{
    return proc.snapToScale (maxNote - (int) std::floor (y / (float) rowH));
}

juce::Rectangle<float> PianoRoll::accentRect (int col) const
{
    const float cx = keyW + (col + 0.5f) * colW();
    return { cx - 12.5f - 11.0f, 359.0f, 22.0f, 22.0f };
}

juce::Rectangle<float> PianoRoll::slideRect (int col) const
{
    const float cx = keyW + (col + 0.5f) * colW();
    return { cx + 12.5f - 11.0f, 359.0f, 22.0f, 22.0f };
}

void PianoRoll::stepParam (const juce::String& id, int delta)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
    {
        const int n = p->choices.size();
        const int v = ((p->getIndex() + delta) % n + n) % n;
        p->setValueNotifyingHost (p->convertTo0to1 ((float) v));
    }
}

void PianoRoll::paint (juce::Graphics& g)
{
    const float W = (float) getWidth();
    const float cw = colW();
    const int root  = (int) proc.apvts.getRawParameterValue ("root")->load();
    const int scale = (int) proc.apvts.getRawParameterValue ("scale")->load();

    g.fillAll (colours::light);

    // Rows
    for (int r = 0; r < numRows; ++r)
    {
        const int n = maxNote - r;
        const int pc = (36 + n) % 12;
        const bool in = proc.inScale (n);
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        const float y = (float) (r * rowH);

        g.setColour (! in ? colours::outScale : (black ? colours::blackKey : colours::light));
        g.fillRect (0.0f, y, W, (float) rowH);
        if (pc == 11 && r > 0) { g.setColour (colours::grid); g.fillRect (0.0f, y, W, 1.0f); }

        const bool isRoot = (((n - root) % 12) + 12) % 12 == 0;
        drawLabel (g, noteName (n), { 4.0f, y, (float) keyW - 4.0f, (float) rowH }, 8.5f,
                   ! in ? colours::faint : (isRoot ? colours::ink : colours::muted2), 0.06f, juce::Justification::centredLeft);
    }

    // Grid lines
    for (int j = 0; j <= numSteps; ++j)
    {
        g.setColour (j % 4 == 0 ? colours::grid : colours::line);
        g.fillRect (keyW + j * cw, 0.0f, 1.0f, (float) rollH);
    }

    // Playhead
    const int play = proc.playStep.load();
    if (play >= 0)
    {
        g.setColour (colours::ink.withAlpha (0.07f));
        g.fillRect (keyW + play * cw, 0.0f, cw, (float) rollH);
    }

    // Notes
    for (int i = 0; i < numSteps; ++i)
    {
        const auto& s = proc.steps[(size_t) i];
        if (! s.on.load()) continue;
        const int note = s.note.load();
        const bool tied = s.slide.load() && i < numSteps - 1 && proc.steps[(size_t) i + 1].on.load();
        const bool acc = s.acc.load();
        juce::Rectangle<float> r (keyW + i * cw + 1.0f, (float) ((maxNote - note) * rowH) + 0.5f, tied ? cw : cw - 3.0f, (float) rowH - 1.0f);

        g.setColour (acc ? colours::ink : colours::noteFill);
        g.fillRect (r);
        g.setColour (colours::ink);
        g.drawRect (r, 1.0f);
        drawLabel (g, noteName (note), r.withTrimmedLeft (3.0f), 9.5f, acc ? colours::light : colours::ink, 0.0f,
                   juce::Justification::centredLeft);
    }

    g.setColour (colours::grid);
    g.fillRect (0.0f, (float) rollH, W, 1.0f);

    // Accent / slide row
    drawLabel (g, juce::String::fromUTF8 ("A \xc2\xb7 S"), { 4.0f, 359.0f, 40.0f, 22.0f }, 8.5f, colours::muted, 0.1f, juce::Justification::centredLeft);
    for (int i = 0; i < numSteps; ++i)
    {
        const auto& s = proc.steps[(size_t) i];
        for (int k = 0; k < 2; ++k)
        {
            const bool on = k == 0 ? s.acc.load() : s.slide.load();
            const auto r = k == 0 ? accentRect (i) : slideRect (i);
            g.setColour (on ? colours::ink : colours::light);
            g.fillEllipse (r);
            g.setColour (colours::ink);
            g.drawEllipse (r.reduced (0.5f), 1.0f);
            drawLabel (g, k == 0 ? "A" : "S", r, 9.0f, on ? colours::light : colours::ink, 0.0f);
        }
    }

    // Scale row
    g.setColour (colours::line);
    g.fillRect (0.0f, 389.0f, W, 1.0f);
    drawLabel (g, "SCALE", { 4.0f, 389.0f, 60.0f, 32.0f }, 8.5f, colours::muted, 0.1f, juce::Justification::centredLeft);

    g.setColour (colours::ink);
    for (auto* r : { &rootPrev, &scalePrev })
        drawChevron (g, juce::Rectangle<float> (8.0f, 12.0f).withCentre (r->toFloat().getCentre()), true, 2.0f);
    for (auto* r : { &rootNext, &scaleNext })
        drawChevron (g, juce::Rectangle<float> (8.0f, 12.0f).withCentre (r->toFloat().getCentre()), false, 2.0f);

    drawLabel (g, pitchClassName (root), { 110.0f, 389.0f, 34.0f, 32.0f }, 11.0f, colours::ink, 0.14f);
    drawLabel (g, scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, scale)].name,
               { 244.0f, 389.0f, 130.0f, 32.0f }, 11.0f, colours::ink, 0.14f);
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    dragStep = -1;

    if (pos.y < rollH && pos.x >= keyW)
    {
        const int col = juce::jlimit (0, numSteps - 1, (int) ((pos.x - keyW) / colW()));
        auto& s = proc.steps[(size_t) col];
        const int pitch = pitchAtY (pos.y);
        dragHit = s.on.load() && s.note.load() == pitch;
        dragMoved = false;
        dragStep = col;
        if (! dragHit) { s.on.store (true); s.note.store (pitch); }
        repaint();
        return;
    }

    for (int i = 0; i < numSteps; ++i)
    {
        if (accentRect (i).expanded (1.5f).contains (pos)) { auto& a = proc.steps[(size_t) i].acc;   a.store (! a.load()); repaint(); return; }
        if (slideRect (i).expanded (1.5f).contains (pos))  { auto& sl = proc.steps[(size_t) i].slide; sl.store (! sl.load()); repaint(); return; }
    }

    const auto p = pos.toInt();
    if (rootPrev.contains (p))  stepParam ("root", -1);
    if (rootNext.contains (p))  stepParam ("root", 1);
    if (scalePrev.contains (p)) stepParam ("scale", -1);
    if (scaleNext.contains (p)) stepParam ("scale", 1);
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStep < 0) return;
    auto& s = proc.steps[(size_t) dragStep];
    const int pitch = pitchAtY (juce::jlimit (0.0f, (float) rollH - 1.0f, e.position.y));
    if (pitch != s.note.load())
    {
        s.note.store (pitch);
        dragMoved = true;
        repaint();
    }
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (dragStep >= 0 && dragHit && ! dragMoved)
        proc.steps[(size_t) dragStep].on.store (false);
    dragStep = -1;
    repaint();
}

//==============================================================================
InfoPanel::InfoPanel()
{
    logo = juce::ImageCache::getFromMemory (BinaryData::acidize_logo_png, BinaryData::acidize_logo_pngSize);
    setSize (990, 228);
}

void InfoPanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::light);
    g.drawImageWithin (logo, 22, 16, 588, 196, juce::RectanglePlacement::centred);

    const juce::Rectangle<float> panel (638.0f, 0.0f, 352.0f, 228.0f);
    g.setColour (colours::panel);
    g.fillRect (panel);

    static const char* const rows[][2] = {
        { "PRODUCT OF", "REZONANZA" }, { "TYPE", "ACID BASS" }, { "VOICES", "1" }, { "KNOBS", "12" },
        { "STEPS", "16" }, { "SCALES", "10" }, { "FILTER", "RESONANT" }, { "TUNING", "A = 440 HZ" },
        { "WAVEFORMS", "SAW / SQR" }
    };
    float y = 18.0f;
    for (const auto& r : rows)
    {
        drawLabel (g, r[0], { panel.getX() + 28.0f, y, 158.0f, 21.0f }, 15.0f, colours::grey, 0.08f, juce::Justification::centredLeft);
        drawLabel (g, r[1], { panel.getX() + 186.0f, y, 150.0f, 21.0f }, 15.0f, colours::ink, 0.08f, juce::Justification::centredLeft);
        y += 21.0f;
    }

    // Close cross
    const auto c = closeArea.toFloat().getCentre();
    g.setColour (colours::ink);
    g.drawLine (c.x - 5.0f, c.y - 5.0f, c.x + 5.0f, c.y + 5.0f, 1.8f);
    g.drawLine (c.x + 5.0f, c.y - 5.0f, c.x - 5.0f, c.y + 5.0f, 1.8f);
}

void InfoPanel::mouseUp (const juce::MouseEvent& e)
{
    if (closeArea.contains (e.getPosition()))
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
}

//==============================================================================
AcidizeView::Knob* AcidizeView::addKnob (const juce::String& id, const juce::String& label)
{
    auto k = std::make_unique<Knob>();
    k->id = id;
    k->label = label;
    auto& s = k->slider;
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setMouseDragSensitivity (200);
    s.setLookAndFeel (&knobLnf);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s);
    if (auto* p = proc.apvts.getParameter (id))
        s.setDoubleClickReturnValue (true, p->getDefaultValue());
    addAndMakeVisible (s);
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

AcidizeView::AcidizeView (AcidizeProcessor& p)
    : proc (p), bars (p), roll (p)
{
    addKnob ("cutoff", "CUTOFF");
    const char* const grid[][2] = { { "tune", "TUNE" }, { "wave", "WAVE" }, { "reso", "RESO" }, { "env", "ENV MOD" }, { "decay", "DECAY" },
                                    { "accent", "ACCENT" }, { "slide", "SLIDE" }, { "drive", "DRIVE" }, { "sub", "SUB" }, { "swing", "SWING" } };
    for (const auto& gk : grid) addKnob (gk[0], gk[1]);
    addKnob ("volume", "OUTPUT");

    playButton.setClickingTogglesState (true);
    playAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "seqOn", playButton);
    addAndMakeVisible (playButton);

    infoButton.onClick = [this] { openInfo(); };
    addAndMakeVisible (infoButton);

    prevButton.onClick = [this] { proc.setCurrentProgram (proc.getCurrentProgram() - 1); roll.repaint(); };
    nextButton.onClick = [this] { proc.setCurrentProgram (proc.getCurrentProgram() + 1); roll.repaint(); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    addAndMakeVisible (bars);
    addChildComponent (roll);

    const bool open = proc.rollOpen.load();
    roll.setVisible (open);
    setSize (width, topH + barH + (open ? PianoRoll::totalH : 0));
    startTimerHz (20);
}

AcidizeView::~AcidizeView()
{
    for (auto& k : knobs) k->slider.setLookAndFeel (nullptr);
}

void AcidizeView::resized()
{
    playButton.setBounds (25, 22, 72, 72);
    knobs[0]->slider.setBounds (25, 120, 72, 72);

    bars.setBounds (146, 27, 606, 120);
    prevButton.setBounds (146, 161, 44, 44);
    nextButton.setBounds (146 + 606 - 44, 161, 44, 44);
    navTextArea = { 190, 161, 518, 44 };

    const float x0 = 776.0f + 8.0f, cellW = (462.0f - 16.0f) / 5.0f;
    for (int i = 0; i < 10; ++i)
    {
        const int col = i % 5, row = i / 5;
        knobs[(size_t) i + 1]->slider.setBounds ((int) std::round (x0 + col * cellW + (cellW - 72.0f) * 0.5f), 22 + row * 98, 72, 72);
    }

    knobs[11]->slider.setBounds (1238 + 25, 22, 72, 72);
    infoButton.setBounds (1238 + 25, 120, 72, 72);

    rollToggleArea = { 0, topH, width, barH };
    rndArea = { width - 12 - 130, topH, 130, barH };
    roll.setBounds (0, topH + barH, width, PianoRoll::totalH);
}

void AcidizeView::paint (juce::Graphics& g)
{
    g.fillAll (colours::light);

    // Side panels
    g.setColour (colours::panel);
    g.fillRect (0, 0, 122, topH);
    g.fillRect (1238, 0, 122, topH);

    // Labels under the round controls
    drawLabel (g, proc.apvts.getRawParameterValue ("seqOn")->load() > 0.5f ? "STOP" : "PLAY", { 0.0f, 100.0f, 122.0f, 13.0f }, 9.5f, colours::ink);
    drawLabel (g, knobs[0]->label, { 0.0f, 198.0f, 122.0f, 13.0f }, 9.5f, colours::ink);
    for (size_t i = 1; i <= 10; ++i)
    {
        const auto b = knobs[i]->slider.getBounds().toFloat();
        drawLabel (g, knobs[i]->label, { b.getCentreX() - 45.0f, b.getBottom() + 6.0f, 90.0f, 13.0f }, 9.5f, colours::ink);
    }
    drawLabel (g, knobs[11]->label, { 1238.0f, 100.0f, 122.0f, 13.0f }, 9.5f, colours::ink);

    // Preset / value readout
    drawLabel (g, centreText(), navTextArea.toFloat(), 16.0f, colours::ink, 0.22f);

    // Piano roll bar
    const bool open = roll.isVisible();
    g.setColour (colours::line);
    g.fillRect (0, topH, width, 1);
    g.fillRect (0, topH + barH - 1, width, 1);

    const auto font = monoFont (9.5f, 0.16f);
    const float textW = juce::GlyphArrangement::getStringWidth (font, "PIANO ROLL");
    const float startX = (width - (12.0f + 10.0f + textW)) * 0.5f;
    const float cy = topH + barH * 0.5f;
    juce::Path chev;
    if (open) { chev.startNewSubPath (startX + 2.0f, cy + 2.5f); chev.lineTo (startX + 6.0f, cy - 2.0f); chev.lineTo (startX + 10.0f, cy + 2.5f); }
    else      { chev.startNewSubPath (startX + 2.0f, cy - 2.5f); chev.lineTo (startX + 6.0f, cy + 2.0f); chev.lineTo (startX + 10.0f, cy - 2.5f); }
    g.setColour (colours::ink);
    g.strokePath (chev, juce::PathStrokeType (1.4f));
    drawLabel (g, "PIANO ROLL", { startX + 22.0f, (float) topH, textW + 4.0f, (float) barH }, 9.5f, colours::ink, 0.16f, juce::Justification::centredLeft);
    drawLabel (g, "RND PATTERN", rndArea.toFloat(), 9.5f, colours::ink, 0.16f, juce::Justification::centredRight);
}

void AcidizeView::mouseUp (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (rndArea.contains (p))
    {
        proc.randomisePattern();
        roll.repaint();
    }
    else if (rollToggleArea.contains (p))
    {
        setRollOpen (! roll.isVisible());
    }
}

void AcidizeView::setRollOpen (bool open)
{
    proc.rollOpen.store (open);
    roll.setVisible (open);
    setSize (width, topH + barH + (open ? PianoRoll::totalH : 0));
    repaint();
    if (onSizeChanged) onSizeChanged();
}

void AcidizeView::openInfo()
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new InfoWindowContent());
    o.dialogTitle = "ACIDIZE";
    o.dialogBackgroundColour = colours::light;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.componentToCentreAround = this;
    o.launchAsync();
}

juce::String AcidizeView::centreText() const
{
    for (const auto& k : knobs)
        if (k->slider.isMouseButtonDown())
            if (auto* param = proc.apvts.getParameter (k->id))
                return k->label + " " + param->getCurrentValueAsText();

    const int idx = proc.currentPreset.load();
    return juce::String (idx + 1).paddedLeft ('0', 2) + " " + AcidizeProcessor::presetNames()[idx];
}

void AcidizeView::timerCallback()
{
    const auto text = centreText() + (proc.apvts.getRawParameterValue ("seqOn")->load() > 0.5f ? "1" : "0");
    if (text != lastCentre)
    {
        lastCentre = text;
        repaint();
    }
}

//==============================================================================
AcidizeEditor::AcidizeEditor (AcidizeProcessor& p)
    : AudioProcessorEditor (&p), view (p)
{
    addAndMakeVisible (view);
    view.setTransform (juce::AffineTransform::scale (scale));
    view.onSizeChanged = [this] { updateSize(); };
    updateSize();
}

void AcidizeEditor::updateSize()
{
    setSize (juce::roundToInt (view.getWidth() * scale), juce::roundToInt (view.getHeight() * scale));
}

InfoWindowContent::InfoWindowContent()
{
    addAndMakeVisible (panel);
    panel.setTransform (juce::AffineTransform::scale (AcidizeEditor::scale));
    setSize (juce::roundToInt (panel.getWidth() * AcidizeEditor::scale), juce::roundToInt (panel.getHeight() * AcidizeEditor::scale));
}

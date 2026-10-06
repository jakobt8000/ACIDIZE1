#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
namespace acid
{
    static const char* const noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const std::vector<ScaleDef>& scales()
    {
        static const std::vector<ScaleDef> s {
            { "CHROMATIC",      { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
            { "MAJOR",          { 0, 2, 4, 5, 7, 9, 11 } },
            { "MINOR",          { 0, 2, 3, 5, 7, 8, 10 } },
            { "DORIAN",         { 0, 2, 3, 5, 7, 9, 10 } },
            { "PHRYGIAN",       { 0, 1, 3, 5, 7, 8, 10 } },
            { "MIXOLYDIAN",     { 0, 2, 4, 5, 7, 9, 10 } },
            { "HARMONIC MINOR", { 0, 2, 3, 5, 7, 8, 11 } },
            { "MINOR PENTA",    { 0, 3, 5, 7, 10 } },
            { "MAJOR PENTA",    { 0, 2, 4, 7, 9 } },
            { "BLUES",          { 0, 3, 5, 6, 7, 10 } }
        };
        return s;
    }

    juce::String pitchClassName (int pc) { return noteNames[((pc % 12) + 12) % 12]; }

    juce::String noteName (int note)
    {
        const int midi = 36 + note;
        return juce::String (noteNames[midi % 12]) + juce::String (midi / 12 - 1);
    }
}

namespace
{
    // { note, gate, accent, slide }
    const int PAT1[16][4] = { {0,1,1,0},{0,1,0,0},{12,1,0,1},{0,1,0,0},{3,1,1,0},{0,0,0,0},{15,1,0,0},{0,1,0,1},
                              {0,1,0,0},{7,1,1,0},{0,1,0,0},{12,1,0,1},{10,1,0,0},{0,0,0,0},{3,1,1,0},{5,1,0,0} };
    const int PAT2[16][4] = { {0,1,0,1},{12,1,1,0},{0,0,0,0},{0,1,0,0},{7,1,0,1},{5,1,1,0},{0,1,0,0},{0,0,0,0},
                              {0,1,1,0},{0,1,0,1},{10,1,0,0},{0,1,0,0},{12,1,1,1},{10,1,0,0},{0,0,0,0},{3,1,0,0} };
    const int PAT3[16][4] = { {0,1,1,0},{0,1,0,0},{0,1,0,0},{12,1,1,0},{0,1,0,0},{0,1,0,1},{1,1,0,0},{0,1,0,0},
                              {0,1,1,0},{0,0,0,0},{13,1,0,1},{12,1,0,0},{0,1,1,0},{0,1,0,0},{7,1,0,1},{6,1,0,0} };

    struct KnobDef { const char* id; const char* name; float def; };
    const KnobDef knobDefs[] = {
        { "cutoff", "Cutoff",  0.30f }, { "tune",   "Tune",    0.50f }, { "wave",   "Wave",    0.00f },
        { "reso",   "Reso",    0.75f }, { "env",    "Env Mod", 0.60f }, { "decay",  "Decay",   0.38f },
        { "accent", "Accent",  0.65f }, { "slide",  "Slide",   0.30f }, { "drive",  "Drive",   0.35f },
        { "sub",    "Sub",     0.00f }, { "swing",  "Swing",   0.00f }, { "volume", "Output",  0.70f }
    };

    struct PresetDef
    {
        const char* name;
        const int (*pattern)[4];
        std::vector<std::pair<const char*, float>> overrides;
    };

    const std::vector<PresetDef>& presets()
    {
        static const std::vector<PresetDef> p {
            { "SQUELCH",   PAT1, {} },
            { "RUBBER",    PAT2, { {"cutoff",0.22f},{"wave",1.0f},{"reso",0.55f},{"env",0.45f},{"decay",0.25f},{"accent",0.5f},{"slide",0.5f},{"drive",0.2f},{"sub",0.5f},{"swing",0.2f} } },
            { "WAREHOUSE", PAT3, { {"cutoff",0.38f},{"reso",0.88f},{"env",0.75f},{"decay",0.55f},{"accent",0.8f},{"drive",0.75f} } },
            { "SUNBURN",   PAT2, { {"cutoff",0.34f},{"wave",0.5f},{"reso",0.65f},{"env",0.55f},{"decay",0.45f},{"drive",0.5f},{"swing",0.35f} } },
            { "LIQUID",    PAT1, { {"cutoff",0.42f},{"reso",0.5f},{"env",0.3f},{"decay",0.7f},{"slide",0.7f},{"drive",0.1f},{"sub",0.3f} } }
        };
        return p;
    }

    float cutoffHz (float v) { return 40.0f * std::exp2 (v * 9.0f); }
    float glideTau (float v) { return 0.008f + v * 0.12f; }

    juce::String formatValue (const juce::String& id, float v)
    {
        if (id == "tune")   { const int st = juce::roundToInt ((v - 0.5f) * 24.0f); return (st > 0 ? "+" : "") + juce::String (st) + " ST"; }
        if (id == "cutoff") { const float hz = cutoffHz (v); return hz < 1000.0f ? juce::String (juce::roundToInt (hz)) + " HZ" : juce::String (hz / 1000.0f, 1) + " KHZ"; }
        if (id == "decay")  return juce::String (juce::roundToInt (30.0f + v * 1200.0f)) + " MS";
        if (id == "slide")  return juce::String (juce::roundToInt (glideTau (v) * 1000.0f)) + " MS";
        if (id == "wave")   return v < 0.03f ? juce::String ("SAW") : v > 0.97f ? juce::String ("SQUARE") : juce::String (juce::roundToInt (v * 100.0f)) + "% SQR";
        return juce::String (juce::roundToInt (v * 100.0f)) + "%";
    }

    inline float polyBlep (float t, float dt)
    {
        if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AcidizeProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& k : knobDefs)
    {
        const juce::String id (k.id);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, k.name, juce::NormalisableRange<float> (0.0f, 1.0f), k.def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([id] (float v, int) { return formatValue (id, v); })));
    }

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "seqOn", 1 }, "Play", false));

    juce::StringArray roots;
    for (int i = 0; i < 12; ++i) roots.add (acid::pitchClassName (i));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "root", 1 }, "Root", roots, 0));

    juce::StringArray scaleNames;
    for (const auto& s : acid::scales()) scaleNames.add (s.name);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "scale", 1 }, "Scale", scaleNames, 0));

    return layout;
}

AcidizeProcessor::AcidizeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ACIDIZE", createLayout())
{
    pCutoff = apvts.getRawParameterValue ("cutoff"); pTune   = apvts.getRawParameterValue ("tune");
    pWave   = apvts.getRawParameterValue ("wave");   pReso   = apvts.getRawParameterValue ("reso");
    pEnv    = apvts.getRawParameterValue ("env");    pDecay  = apvts.getRawParameterValue ("decay");
    pAccent = apvts.getRawParameterValue ("accent"); pSlide  = apvts.getRawParameterValue ("slide");
    pDrive  = apvts.getRawParameterValue ("drive");  pSub    = apvts.getRawParameterValue ("sub");
    pSwing  = apvts.getRawParameterValue ("swing");  pVolume = apvts.getRawParameterValue ("volume");
    pSeqOn  = apvts.getRawParameterValue ("seqOn");
    pRoot   = apvts.getRawParameterValue ("root");   pScale  = apvts.getRawParameterValue ("scale");

    loadPattern (PAT1);
}

bool AcidizeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void AcidizeProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    amp = ampTarget = 0.0f;
    ic1 = ic2 = 0.0f;
    gate = false;
    lastStep = -1;
    heldNotes.clear();
}

//==============================================================================
// Voice

void AcidizeProcessor::voiceNoteOn (int note, float accent, bool tie)
{
    const int semi = 36 + note + juce::roundToInt ((tune - 0.5f) * 24.0f);
    targetFreq = 440.0f * std::exp2 ((float) (semi - 69) / 12.0f);

    if (! tie)
    {
        freq = targetFreq;
        filterEnv = 1.0f;
        accentNow = accent;
    }

    gate = true;
    ampTarget = 0.3f * (1.0f + accentNow * 0.6f);
}

void AcidizeProcessor::voiceGateOff()
{
    gate = false;
    ampTarget = 0.0f;
}

void AcidizeProcessor::triggerStep (int index)
{
    const auto& s = steps[(size_t) index];
    const auto& prev = steps[(size_t) ((index + acid::numSteps - 1) % acid::numSteps)];
    const bool tie = lastStep >= 0 && gate && prev.on.load() && prev.slide.load();

    playStep.store (index);

    if (! s.on.load())
    {
        voiceGateOff();
        return;
    }

    voiceNoteOn (s.note.load(), s.acc.load() ? accentAmt : 0.0f, tie);
}

float AcidizeProcessor::renderSample()
{
    const float fs = (float) sr;

    // Glide
    const float glideCoef = 1.0f - std::exp (-1.0f / (glideTau (slide) * fs));
    freq += (targetFreq - freq) * glideCoef;

    // Oscillators
    const float dt = juce::jlimit (0.0f, 0.45f, freq / fs);
    phase += dt; if (phase >= 1.0f) phase -= 1.0f;
    const float subDt = dt * 0.5f;
    subPhase += subDt; if (subPhase >= 1.0f) subPhase -= 1.0f;

    const float saw = 2.0f * phase - 1.0f - polyBlep (phase, dt);
    float sqr = phase < 0.5f ? 1.0f : -1.0f;
    sqr += polyBlep (phase, dt);
    sqr -= polyBlep (std::fmod (phase + 0.5f, 1.0f), dt);
    float subSq = subPhase < 0.5f ? 1.0f : -1.0f;
    subSq += polyBlep (subPhase, subDt);
    subSq -= polyBlep (std::fmod (subPhase + 0.5f, 1.0f), subDt);

    const float osc = saw * (1.0f - wave) * 0.8f + sqr * wave * 0.55f + subSq * sub * 0.5f;

    // Filter envelope
    const float decTime = (0.03f + decay * 1.2f * (accentNow > 0.0f ? 0.6f : 1.0f)) / 3.0f;
    filterEnv *= std::exp (-1.0f / (decTime * fs));
    const float base = cutoffHz (cutoff);
    const float fc = juce::jmin (18000.0f, fs * 0.45f, base * std::exp2 (filterEnv * (envMod * 4.5f + accentNow * 1.5f)));

    // Resonant low-pass (state-variable, trapezoidal)
    const float g = std::tan (juce::MathConstants<float>::pi * fc / fs);
    const float k = 1.0f / (0.7f + reso * 14.0f);
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;
    const float v3 = osc - ic2;
    const float v1 = a1 * ic1 + a2 * v3;
    const float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;

    // Drive
    const float driven = std::tanh (driveK * v2) * driveNorm;

    // Amp
    const float ampTime = ampTarget > amp ? 0.0015f : 0.008f;   // fast attack, short release
    amp += (ampTarget - amp) * (1.0f - std::exp (-1.0f / (ampTime * fs)));

    return driven * amp * volume * 0.8f;
}

//==============================================================================
void AcidizeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    cutoff = pCutoff->load(); tune = pTune->load(); wave = pWave->load(); reso = pReso->load();
    envMod = pEnv->load(); decay = pDecay->load(); accentAmt = pAccent->load(); slide = pSlide->load();
    drive = pDrive->load(); sub = pSub->load(); swing = pSwing->load(); volume = pVolume->load();
    driveK = 1.0f + drive * 25.0f;
    driveNorm = 1.0f / std::tanh (driveK);

    double bpm = 124.0;
    bool hostPlaying = false;
    juce::Optional<double> ppq;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            hostPlaying = pos->getIsPlaying();
            ppq = pos->getPpqPosition();
        }

    const bool run = pSeqOn->load() > 0.5f;

    if (run && ! seqWasRunning)
    {
        seqPos16 = 0.0;
        lastStep = -1;
    }
    if (! run && seqWasRunning)
    {
        voiceGateOff();
        lastStep = -1;
        playStep.store (-1);
    }
    seqWasRunning = run;

    if (run && hostPlaying && ppq.hasValue())
        seqPos16 = *ppq * 4.0;   // follow the DAW's timeline

    const double inc16 = bpm / 60.0 * 4.0 / sr;
    const double sw = swing * 0.5;
    const double boundary = 1.0 + sw;

    auto midiIt = midi.cbegin();
    const int numSamples = buffer.getNumSamples();
    auto* left = buffer.getWritePointer (0);

    for (int n = 0; n < numSamples; ++n)
    {
        // MIDI plays the synth directly when the sequencer is stopped
        while (midiIt != midi.cend() && (*midiIt).samplePosition <= n)
        {
            const auto msg = (*midiIt).getMessage();
            if (! run)
            {
                if (msg.isNoteOn())
                {
                    const bool tie = gate && ! heldNotes.empty();
                    heldNotes.push_back (msg.getNoteNumber());
                    voiceNoteOn (msg.getNoteNumber() - 36, msg.getVelocity() > 100 ? accentAmt : 0.0f, tie);
                }
                else if (msg.isNoteOff())
                {
                    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), msg.getNoteNumber()), heldNotes.end());
                    if (heldNotes.empty()) voiceGateOff();
                    else voiceNoteOn (heldNotes.back() - 36, accentNow, true);
                }
                else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                {
                    heldNotes.clear();
                    voiceGateOff();
                }
            }
            ++midiIt;
        }

        if (run)
        {
            const double pair = std::floor (seqPos16 / 2.0);
            const double p = seqPos16 - pair * 2.0;
            const bool second = p >= boundary;
            const int idx = (int) ((((long long) pair * 2 + (second ? 1 : 0)) % acid::numSteps + acid::numSteps) % acid::numSteps);
            const double frac = second ? (p - boundary) / (2.0 - boundary) : p / boundary;

            if (idx != lastStep)
            {
                triggerStep (idx);
                lastStep = idx;
            }

            if (gate && frac >= 0.6 && ! steps[(size_t) idx].slide.load())
                voiceGateOff();

            seqPos16 += inc16;
        }

        left[n] = renderSample();
    }

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

    // Analyser
    int w = ringWrite.load();
    for (int n = 0; n < numSamples; ++n)
    {
        ring[(size_t) w] = left[n];
        w = (w + 1) & (ringSize - 1);
    }
    ringWrite.store (w);
}

void AcidizeProcessor::copyRecentOutput (float* dest, int num) const
{
    num = juce::jmin (num, ringSize);
    const int w = ringWrite.load();
    for (int i = 0; i < num; ++i)
        dest[i] = ring[(size_t) ((w - num + i + ringSize) & (ringSize - 1))];
}

//==============================================================================
// Pattern helpers

bool AcidizeProcessor::inScale (int note) const
{
    const int root = (int) pRoot->load();
    const auto& pcs = acid::scales()[(size_t) juce::jlimit (0, (int) acid::scales().size() - 1, (int) pScale->load())].pitchClasses;
    const int pc = (((note - root) % 12) + 12) % 12;
    return std::find (pcs.begin(), pcs.end(), pc) != pcs.end();
}

int AcidizeProcessor::snapToScale (int note) const
{
    note = juce::jlimit (acid::minNote, acid::maxNote, note);
    if (inScale (note)) return note;
    for (int d = 1; d < 12; ++d)
    {
        if (note - d >= acid::minNote && inScale (note - d)) return note - d;
        if (note + d <= acid::maxNote && inScale (note + d)) return note + d;
    }
    return note;
}

void AcidizeProcessor::snapPatternToScale()
{
    for (auto& s : steps)
        s.note.store (snapToScale (s.note.load()));
}

void AcidizeProcessor::randomisePattern()
{
    static const int pool[] = { 0, 0, 0, 0, 12, 3, 5, 7, 10, 15, 12, 24 };
    for (int i = 0; i < acid::numSteps; ++i)
    {
        auto& s = steps[(size_t) i];
        s.note.store (snapToScale (pool[rng.nextInt ((int) std::size (pool))]));
        s.on.store (i == 0 || rng.nextFloat() < 0.8f);
        s.acc.store (rng.nextFloat() < 0.3f);
        s.slide.store (rng.nextFloat() < 0.25f);
    }
}

void AcidizeProcessor::loadPattern (const int pattern[acid::numSteps][4])
{
    for (int i = 0; i < acid::numSteps; ++i)
    {
        auto& s = steps[(size_t) i];
        s.note.store (pattern[i][0]);
        s.on.store (pattern[i][1] != 0);
        s.acc.store (pattern[i][2] != 0);
        s.slide.store (pattern[i][3] != 0);
    }
}

//==============================================================================
// Presets

juce::StringArray AcidizeProcessor::presetNames()
{
    juce::StringArray names;
    for (const auto& p : presets()) names.add (p.name);
    return names;
}

int AcidizeProcessor::getNumPrograms() { return (int) presets().size(); }
int AcidizeProcessor::getCurrentProgram() { return currentPreset.load(); }
const juce::String AcidizeProcessor::getProgramName (int index) { return presetNames()[index]; }

void AcidizeProcessor::setParamPlain (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void AcidizeProcessor::setCurrentProgram (int index)
{
    const auto& all = presets();
    index = ((index % (int) all.size()) + (int) all.size()) % (int) all.size();
    const auto& pr = all[(size_t) index];

    for (const auto& k : knobDefs)
    {
        if (juce::String (k.id) == "volume") continue;  // keep the user's output level
        float v = k.def;
        for (const auto& o : pr.overrides)
            if (juce::String (o.first) == k.id) v = o.second;
        setParamPlain (k.id, v);
    }

    loadPattern (pr.pattern);
    snapPatternToScale();
    currentPreset.store (index);
}

//==============================================================================
// State

void AcidizeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    state.setProperty ("rollOpen", rollOpen.load(), nullptr);

    state.removeChild (state.getChildWithName ("PATTERN"), nullptr);
    juce::ValueTree pattern ("PATTERN");
    for (const auto& s : steps)
    {
        juce::ValueTree st ("STEP");
        st.setProperty ("note", s.note.load(), nullptr);
        st.setProperty ("on", s.on.load(), nullptr);
        st.setProperty ("acc", s.acc.load(), nullptr);
        st.setProperty ("slide", s.slide.load(), nullptr);
        pattern.appendChild (st, nullptr);
    }
    state.appendChild (pattern, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AcidizeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType())) return;

    currentPreset.store ((int) state.getProperty ("preset", 0));
    rollOpen.store ((bool) state.getProperty ("rollOpen", true));

    auto pattern = state.getChildWithName ("PATTERN");
    for (int i = 0; i < pattern.getNumChildren() && i < acid::numSteps; ++i)
    {
        auto st = pattern.getChild (i);
        auto& s = steps[(size_t) i];
        s.note.store (juce::jlimit (acid::minNote, acid::maxNote, (int) st.getProperty ("note", 0)));
        s.on.store ((bool) st.getProperty ("on", false));
        s.acc.store ((bool) st.getProperty ("acc", false));
        s.slide.store ((bool) st.getProperty ("slide", false));
    }

    state.removeChild (pattern, nullptr);
    apvts.replaceState (state);
}

//==============================================================================
juce::AudioProcessorEditor* AcidizeProcessor::createEditor()
{
    return new AcidizeEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AcidizeProcessor();
}

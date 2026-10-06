#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

namespace acid
{
    // One step of the 16-step pattern. Notes are semitones above C2 (0 = C2, 24 = C4).
    struct Step
    {
        std::atomic<int>  note  { 0 };
        std::atomic<bool> on    { false };
        std::atomic<bool> acc   { false };
        std::atomic<bool> slide { false };
    };

    struct ScaleDef
    {
        const char* name;
        std::vector<int> pitchClasses;
    };

    const std::vector<ScaleDef>& scales();
    juce::String noteName (int note);          // note relative to C2
    juce::String pitchClassName (int pc);

    constexpr int numSteps = 16;
    constexpr int minNote  = 0;
    constexpr int maxNote  = 24;
}

class AcidizeProcessor : public juce::AudioProcessor
{
public:
    AcidizeProcessor();
    ~AcidizeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ACIDIZE"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    std::array<acid::Step, acid::numSteps> steps;
    std::atomic<int>  playStep { -1 };
    std::atomic<int>  currentPreset { 0 };
    std::atomic<bool> rollOpen { true };   // UI state, remembered with the session

    // Pattern helpers (message thread)
    bool inScale (int note) const;
    int  snapToScale (int note) const;
    void snapPatternToScale();
    void randomisePattern();

    // Analyser: copies the most recent `num` output samples into dest
    void copyRecentOutput (float* dest, int num) const;

    static juce::StringArray presetNames();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParamPlain (const juce::String& id, float value);
    void loadPattern (const int pattern[acid::numSteps][4]);

    // Voice
    void triggerStep (int index);
    void voiceNoteOn (int note, float accent, bool tie);
    void voiceGateOff();
    float renderSample();

    double sr = 44100.0;
    float phase = 0.0f, subPhase = 0.0f;
    float freq = 65.4f, targetFreq = 65.4f;
    float amp = 0.0f, ampTarget = 0.0f;
    float filterEnv = 0.0f;
    float accentNow = 0.0f;
    bool  gate = false;
    float ic1 = 0.0f, ic2 = 0.0f;

    // Sequencer
    double seqPos16 = 0.0;
    int lastStep = -1;
    bool seqWasRunning = false;

    // MIDI play (when the sequencer is stopped)
    std::vector<int> heldNotes;

    // Cached parameter pointers
    std::atomic<float>* pCutoff = nullptr; std::atomic<float>* pTune = nullptr;
    std::atomic<float>* pWave = nullptr;   std::atomic<float>* pReso = nullptr;
    std::atomic<float>* pEnv = nullptr;    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pAccent = nullptr; std::atomic<float>* pSlide = nullptr;
    std::atomic<float>* pDrive = nullptr;  std::atomic<float>* pSub = nullptr;
    std::atomic<float>* pSwing = nullptr;  std::atomic<float>* pVolume = nullptr;
    std::atomic<float>* pSeqOn = nullptr;
    std::atomic<float>* pRoot = nullptr;   std::atomic<float>* pScale = nullptr;

    // Per-block parameter snapshot
    float cutoff = 0.3f, tune = 0.5f, wave = 0.0f, reso = 0.75f, envMod = 0.6f, decay = 0.38f,
          accentAmt = 0.65f, slide = 0.3f, drive = 0.35f, sub = 0.0f, swing = 0.0f, volume = 0.7f;
    float driveNorm = 1.0f, driveK = 1.0f;

    // Analyser ring buffer
    static constexpr int ringSize = 8192;
    std::array<float, ringSize> ring {};
    std::atomic<int> ringWrite { 0 };

    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidizeProcessor)
};

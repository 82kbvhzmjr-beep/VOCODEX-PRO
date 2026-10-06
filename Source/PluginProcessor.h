#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>
#include "Dsp.h"

struct PresetDef
{
    int category;                                           // 0..3
    const char* name;
    std::vector<std::pair<const char*, float>> values;      // id de parametro -> valor real
};

const std::vector<PresetDef>& getPresetList();
const char* getCategoryName (int index);                    // texto en UTF-8
constexpr int kNumCategories = 4;

class VocodexProcessor : public juce::AudioProcessor
{
public:
    VocodexProcessor();
    ~VocodexProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Vocodex Pro"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void applyPreset (int presetIndex);
    float getDetectedMidi() const { return engine.detectedMidi.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    vx::Params readParams() const;

    vx::Engine engine;
    double sampleRateHz = 44100.0;

    std::atomic<float>* pLowCut; std::atomic<float>* pEqLow; std::atomic<float>* pEqMid; std::atomic<float>* pEqHigh;
    std::atomic<float>* pComp; std::atomic<float>* pCompMode; std::atomic<float>* pDeess; std::atomic<float>* pDeessFocus;
    std::atomic<float>* pTuneOn; std::atomic<float>* pKey; std::atomic<float>* pScale;
    std::atomic<float>* pTuneSpeed; std::atomic<float>* pTuneAmount; std::atomic<float>* pTuneTol;
    std::atomic<float>* pToneType; std::atomic<float>* pToneInt; std::atomic<float>* pMagic;
    std::atomic<float>* pMidAir; std::atomic<float>* pHighAir;
    std::atomic<float>* pMic; std::atomic<float>* pMicBody; std::atomic<float>* pMicAir;
    std::atomic<float>* pRevType; std::atomic<float>* pRevMix; std::atomic<float>* pRevLow; std::atomic<float>* pRevHigh;
    std::atomic<float>* pDlyTime; std::atomic<float>* pDlyMix; std::atomic<float>* pDlyFb;
    std::atomic<float>* pDlyLow; std::atomic<float>* pDlyHigh;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocodexProcessor)
};

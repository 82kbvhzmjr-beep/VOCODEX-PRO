#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

static juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

//==============================================================================
// Categorias y presets
//==============================================================================
const char* getCategoryName (int index)
{
    static const char* names[kNumCategories] = { "CANTANTES", "TRAP", "REGGAET\xC3\x93N", "R&B" };
    return names[juce::jlimit (0, kNumCategories - 1, index)];
}

// Orden de mic: 0 Manley, 1 Neumann, 2 C414, 3 NT1A, 4 AT2020, 5 P220, 6 e835, 7 Yeti
// Orden de toneType: 0 Warm, 1 Agudo, 2 Saturacion
// compMode: 0 Smooth, 1 Punchy, 2 Aggressive
// revType: 0 Plate, 1 Room, 2 Large      dlyTime: 0 1/16, 1 1/8, 2 1/4
const std::vector<PresetDef>& getPresetList()
{
    static const std::vector<PresetDef> list = {
        { 0, "Quevedo", {
            { "tuneOn", 1 }, { "tuneSpeed", 18 }, { "tuneAmount", 92 }, { "tuneTol", 8 },
            { "comp", 58 }, { "compMode", 1 }, { "deess", 45 }, { "deessFocus", 7000 },
            { "lowCut", 80 }, { "eqMid", -2.5f }, { "eqLow", 1.0f }, { "eqHigh", 1.5f },
            { "toneType", 2 }, { "toneInt", 18 }, { "magic", 18 },
            { "midAir", 20 }, { "highAir", 25 }, { "mic", 1 },
            { "revType", 0 }, { "revMix", 14 }, { "dlyTime", 1 }, { "dlyMix", 10 }, { "dlyFb", 28 } } },

        { 1, "Trap Moderno", {
            { "tuneOn", 1 }, { "tuneSpeed", 8 }, { "tuneAmount", 100 }, { "tuneTol", 5 },
            { "comp", 62 }, { "compMode", 1 }, { "deess", 50 }, { "deessFocus", 7500 },
            { "lowCut", 90 }, { "eqLow", 1.5f }, { "eqMid", -2 }, { "eqHigh", 2.5f },
            { "toneType", 1 }, { "toneInt", 22 }, { "magic", 25 },
            { "midAir", 25 }, { "highAir", 35 }, { "mic", 1 },
            { "revType", 0 }, { "revMix", 12 }, { "dlyTime", 1 }, { "dlyMix", 12 }, { "dlyFb", 28 } } },
        { 1, "Trap Oscuro", {
            { "tuneOn", 1 }, { "tuneSpeed", 12 }, { "tuneAmount", 95 }, { "tuneTol", 8 },
            { "comp", 60 }, { "compMode", 1 }, { "deess", 40 }, { "deessFocus", 6500 },
            { "lowCut", 75 }, { "eqLow", 3 }, { "eqMid", -3 }, { "eqHigh", -1 },
            { "toneType", 2 }, { "toneInt", 28 }, { "magic", 20 },
            { "midAir", 10 }, { "highAir", 15 }, { "mic", 0 },
            { "revType", 2 }, { "revMix", 18 }, { "revHigh", 6000 }, { "dlyTime", 2 }, { "dlyMix", 14 }, { "dlyFb", 35 } } },
        { 1, "Trap Mel\xC3\xB3" "dico", {
            { "tuneOn", 1 }, { "tuneSpeed", 5 }, { "tuneAmount", 100 }, { "tuneTol", 3 },
            { "comp", 55 }, { "compMode", 0 }, { "deess", 45 }, { "deessFocus", 7500 },
            { "lowCut", 85 }, { "eqLow", 1 }, { "eqMid", -1.5f }, { "eqHigh", 3 },
            { "toneType", 0 }, { "toneInt", 20 }, { "magic", 30 },
            { "midAir", 30 }, { "highAir", 40 }, { "mic", 2 },
            { "revType", 0 }, { "revMix", 20 }, { "dlyTime", 1 }, { "dlyMix", 18 }, { "dlyFb", 40 } } },

        { 2, "Reggaet\xC3\xB3n Limpio", {
            { "tuneOn", 1 }, { "tuneSpeed", 25 }, { "tuneAmount", 80 }, { "tuneTol", 12 },
            { "comp", 50 }, { "compMode", 0 }, { "deess", 50 }, { "deessFocus", 7000 },
            { "lowCut", 90 }, { "eqMid", 0.5f }, { "eqHigh", 2 },
            { "toneType", 0 }, { "toneInt", 10 }, { "magic", 15 },
            { "midAir", 20 }, { "highAir", 25 }, { "mic", 1 },
            { "revType", 1 }, { "revMix", 8 }, { "dlyTime", 0 }, { "dlyMix", 6 }, { "dlyFb", 20 } } },
        { 2, "Reggaet\xC3\xB3n Amplio", {
            { "tuneOn", 1 }, { "tuneSpeed", 20 }, { "tuneAmount", 85 }, { "tuneTol", 10 },
            { "comp", 55 }, { "compMode", 0 }, { "deess", 48 }, { "deessFocus", 7000 },
            { "lowCut", 80 }, { "eqLow", 1 }, { "eqMid", -1 }, { "eqHigh", 2.5f },
            { "toneType", 0 }, { "toneInt", 15 }, { "magic", 28 },
            { "midAir", 30 }, { "highAir", 35 }, { "mic", 0 },
            { "revType", 2 }, { "revMix", 18 }, { "dlyTime", 1 }, { "dlyMix", 14 }, { "dlyFb", 30 } } },

        { 3, "R&B Velvet", {
            { "tuneOn", 1 }, { "tuneSpeed", 40 }, { "tuneAmount", 70 }, { "tuneTol", 15 },
            { "comp", 45 }, { "compMode", 0 }, { "deess", 45 }, { "deessFocus", 6500 },
            { "lowCut", 70 }, { "eqLow", 2 }, { "eqMid", -1 }, { "eqHigh", 1 },
            { "toneType", 0 }, { "toneInt", 25 }, { "magic", 22 },
            { "midAir", 25 }, { "highAir", 30 }, { "mic", 0 },
            { "revType", 0 }, { "revMix", 18 }, { "dlyTime", 1 }, { "dlyMix", 8 }, { "dlyFb", 25 } } },
        { 3, "R&B Intimo", {
            { "tuneOn", 1 }, { "tuneSpeed", 60 }, { "tuneAmount", 55 }, { "tuneTol", 20 },
            { "comp", 40 }, { "compMode", 0 }, { "deess", 40 }, { "deessFocus", 6500 },
            { "lowCut", 100 }, { "eqLow", 1.5f }, { "eqHigh", 0.5f },
            { "toneType", 0 }, { "toneInt", 20 }, { "magic", 15 },
            { "midAir", 15 }, { "highAir", 20 }, { "mic", 2 },
            { "revType", 1 }, { "revMix", 10 }, { "dlyMix", 0 } } },
        { 3, "R&B Dream", {
            { "tuneOn", 1 }, { "tuneSpeed", 35 }, { "tuneAmount", 75 }, { "tuneTol", 12 },
            { "comp", 48 }, { "compMode", 0 }, { "deess", 48 }, { "deessFocus", 7000 },
            { "lowCut", 85 }, { "eqMid", -1.5f }, { "eqHigh", 2 },
            { "toneType", 0 }, { "toneInt", 18 }, { "magic", 35 },
            { "midAir", 40 }, { "highAir", 45 }, { "mic", 1 },
            { "revType", 2 }, { "revMix", 30 }, { "revLow", 250 }, { "revHigh", 9000 },
            { "dlyTime", 2 }, { "dlyMix", 20 }, { "dlyFb", 40 } } },
    };
    return list;
}

//==============================================================================
// Parametros
//==============================================================================
APVTS::ParameterLayout VocodexProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto F = [&] (const char* id, const char* name, float lo, float hi, float step, float def, float skew = 1.0f)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, step, skew), def));
    };
    auto C = [&] (const char* id, const char* name, std::initializer_list<juce::String> list, int def)
    {
        juce::StringArray items;
        for (const auto& s : list) items.add (s);
        p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
    };

    // EQ
    F ("lowCut", "Low Cut", 20.f, 400.f, 1.f, 20.f, 0.5f);
    F ("eqLow",  "EQ Low",  -12.f, 12.f, 0.1f, 0.f);
    F ("eqMid",  "EQ Mid",  -12.f, 12.f, 0.1f, 0.f);
    F ("eqHigh", "EQ High", -12.f, 12.f, 0.1f, 0.f);
    // Dynamics
    F ("comp", "Comp", 0.f, 100.f, 1.f, 0.f);
    C ("compMode", "Comp Mode", { "Smooth", "Punchy", "Aggressive" }, 0);
    F ("deess", "De-Ess", 0.f, 100.f, 1.f, 0.f);
    F ("deessFocus", "De-Ess Focus", 4000.f, 10000.f, 10.f, 7000.f);
    // Tune
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "tuneOn", 1 }, "Tune On", false));
    C ("key", "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
    C ("scale", "Scale", { "Chromatic", "Major", "Minor", "Pentatonic Minor" }, 0);
    F ("tuneSpeed", "Tune Speed", 0.f, 200.f, 1.f, 20.f);
    F ("tuneAmount", "Tune Amount", 0.f, 100.f, 1.f, 100.f);
    F ("tuneTol", "Tune Tolerance", 0.f, 50.f, 1.f, 10.f);
    // Tone
    C ("toneType", "Tone Type", { "Warm", "Agudo", U ("Saturaci\xC3\xB3n") }, 0);
    F ("toneInt", "Tone Intensidad", 0.f, 100.f, 1.f, 0.f);
    F ("magic", "Magic", 0.f, 100.f, 1.f, 0.f);
    // Air
    F ("midAir", "Mid Air", 0.f, 100.f, 1.f, 0.f);
    F ("highAir", "High Air", 0.f, 100.f, 1.f, 0.f);
    // Mic
    C ("mic", "Mic", { "Manley-Style Tube", "Neumann TLM 103-Style", "AKG C414 XLII-Style",
                       U ("R\xC3\x98" "DE NT1A-Style"), "Audio-Technica AT2020-Style", "AKG P220-Style",
                       "Sennheiser e835-Style", "Blue Yeti-Style" }, 1);
    F ("micBody", "Mic Body", 0.f, 100.f, 1.f, 50.f);
    F ("micAir", "Mic Air", 0.f, 100.f, 1.f, 50.f);
    // Reverb
    C ("revType", "Reverb Type", { "Plate", "Room", "Large" }, 0);
    F ("revMix", "Reverb Mix", 0.f, 100.f, 1.f, 0.f);
    F ("revLow", "Reverb Low Cut", 20.f, 1000.f, 1.f, 150.f, 0.5f);
    F ("revHigh", "Reverb High Cut", 1000.f, 20000.f, 10.f, 10000.f, 0.5f);
    // Delay
    C ("dlyTime", "Delay Time", { "1/16", "1/8", "1/4" }, 1);
    F ("dlyMix", "Delay Mix", 0.f, 100.f, 1.f, 0.f);
    F ("dlyFb", "Delay Feedback", 0.f, 90.f, 1.f, 30.f);
    F ("dlyLow", "Delay Low Cut", 20.f, 1000.f, 1.f, 150.f, 0.5f);
    F ("dlyHigh", "Delay High Cut", 1000.f, 20000.f, 10.f, 8000.f, 0.5f);

    return { p.begin(), p.end() };
}

//==============================================================================
VocodexProcessor::VocodexProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VOCODEX", createLayout())
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pLowCut = g ("lowCut"); pEqLow = g ("eqLow"); pEqMid = g ("eqMid"); pEqHigh = g ("eqHigh");
    pComp = g ("comp"); pCompMode = g ("compMode"); pDeess = g ("deess"); pDeessFocus = g ("deessFocus");
    pTuneOn = g ("tuneOn"); pKey = g ("key"); pScale = g ("scale");
    pTuneSpeed = g ("tuneSpeed"); pTuneAmount = g ("tuneAmount"); pTuneTol = g ("tuneTol");
    pToneType = g ("toneType"); pToneInt = g ("toneInt"); pMagic = g ("magic");
    pMidAir = g ("midAir"); pHighAir = g ("highAir");
    pMic = g ("mic"); pMicBody = g ("micBody"); pMicAir = g ("micAir");
    pRevType = g ("revType"); pRevMix = g ("revMix"); pRevLow = g ("revLow"); pRevHigh = g ("revHigh");
    pDlyTime = g ("dlyTime"); pDlyMix = g ("dlyMix"); pDlyFb = g ("dlyFb");
    pDlyLow = g ("dlyLow"); pDlyHigh = g ("dlyHigh");
}

bool VocodexProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VocodexProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
}

vx::Params VocodexProcessor::readParams() const
{
    vx::Params p;
    p.lowCut = pLowCut->load(); p.eqLow = pEqLow->load(); p.eqMid = pEqMid->load(); p.eqHigh = pEqHigh->load();
    p.comp = pComp->load(); p.compMode = (int) pCompMode->load();
    p.deess = pDeess->load(); p.deessFocus = pDeessFocus->load();
    p.tuneOn = pTuneOn->load() > 0.5f; p.key = (int) pKey->load(); p.scale = (int) pScale->load();
    p.tuneSpeed = pTuneSpeed->load(); p.tuneAmount = pTuneAmount->load(); p.tuneTol = pTuneTol->load();
    p.toneType = (int) pToneType->load(); p.toneInt = pToneInt->load(); p.magic = pMagic->load();
    p.midAir = pMidAir->load(); p.highAir = pHighAir->load();
    p.mic = (int) pMic->load(); p.micBody = pMicBody->load(); p.micAir = pMicAir->load();
    p.revType = (int) pRevType->load(); p.revMix = pRevMix->load();
    p.revLow = pRevLow->load(); p.revHigh = pRevHigh->load();
    p.dlyTime = (int) pDlyTime->load(); p.dlyMix = pDlyMix->load(); p.dlyFb = pDlyFb->load();
    p.dlyLow = pDlyLow->load(); p.dlyHigh = pDlyHigh->load();
    return p;
}

void VocodexProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    if (numCh < 1 || n < 1) return;

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    engine.process (L, R, n, bpm, readParams());
}

//==============================================================================
void VocodexProcessor::applyPreset (int presetIndex)
{
    const auto& list = getPresetList();
    if (presetIndex < 0 || presetIndex >= (int) list.size()) return;
    const auto& pr = list[(size_t) presetIndex];

    // 1) todo a valores por defecto (menos tonalidad y escala, que elige el usuario)
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            if (rp->paramID == "key" || rp->paramID == "scale") continue;
            rp->setValueNotifyingHost (rp->getDefaultValue());
        }

    // 2) valores del preset
    for (const auto& kv : pr.values)
        if (auto* rp = apvts.getParameter (kv.first))
            rp->setValueNotifyingHost (rp->convertTo0to1 (kv.second));

    apvts.state.setProperty ("presetName", juce::String::fromUTF8 (pr.name), nullptr);
    apvts.state.setProperty ("presetCat", pr.category, nullptr);
}

//==============================================================================
void VocodexProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocodexProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VocodexProcessor::createEditor() { return new VocodexEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocodexProcessor(); }

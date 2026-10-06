#include "PluginEditor.h"

using namespace vxui;

namespace
{
constexpr int kMargin = 14;
constexpr int kHeaderH = 64;
constexpr int kContentTop = 146;

juce::String noteText (float m)
{
    if (m < 0.0f) return "--";
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int r = (int) std::floor (m + 0.5f);
    const int cents = (int) std::lround ((m - (float) r) * 100.0f);
    const int idx = ((r % 12) + 12) % 12;
    const int oct = r / 12 - 1;
    return juce::String (names[idx]) + juce::String (oct) + " " + (cents >= 0 ? "+" : "") + juce::String (cents) + "c";
}
}

//==============================================================================
KnobComp* VocodexEditor::addKnob (const char* id, const char* name, const char* suffix, int decimals)
{
    auto* k = new KnobComp (proc.apvts, id, name, suffix, decimals);
    advComps.add (k);
    addChildComponent (k);
    return k;
}

ComboComp* VocodexEditor::addCombo (const char* id, const char* name)
{
    auto* c = new ComboComp (proc.apvts, id, name);
    advComps.add (c);
    addChildComponent (c);
    return c;
}

//==============================================================================
VocodexEditor::VocodexEditor (VocodexProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);

    //---------------- cabecera ----------------
    simpleBtn.setButtonText ("SIMPLE");
    advBtn.setButtonText ("ADVANCED");
    for (auto* b : { &simpleBtn, &advBtn })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1);
        addAndMakeVisible (b);
    }
    simpleBtn.onClick = [this] { if (simpleBtn.getToggleState()) setAdvanced (false); };
    advBtn.onClick    = [this] { if (advBtn.getToggleState())    setAdvanced (true);  };

    //---------------- categorias ----------------
    selectedCat = juce::jlimit (0, kNumCategories - 1, (int) proc.apvts.state.getProperty ("presetCat", 0));
    for (int i = 0; i < kNumCategories; ++i)
    {
        catBtns[i].setButtonText (juce::String::fromUTF8 (getCategoryName (i)));
        catBtns[i].setClickingTogglesState (true);
        catBtns[i].setRadioGroupId (2);
        catBtns[i].onClick = [this, i]
        {
            if (! catBtns[i].getToggleState()) return;
            selectedCat = i;
            rebuildPresetButtons();
            resized();
        };
        addAndMakeVisible (catBtns[i]);
    }
    catBtns[selectedCat].setToggleState (true, juce::dontSendNotification);
    rebuildPresetButtons();

    //---------------- SIMPLE: 4 controles grandes ----------------
    sComp  = std::make_unique<KnobComp> (proc.apvts, "comp",   "COMP",   "%", 0);
    sMagic = std::make_unique<KnobComp> (proc.apvts, "magic",  "MAGIC",  "%", 0);
    sRev   = std::make_unique<KnobComp> (proc.apvts, "revMix", "REVERB", "%", 0);
    sDly   = std::make_unique<KnobComp> (proc.apvts, "dlyMix", "DELAY",  "%", 0);
    for (auto* k : { sComp.get(), sMagic.get(), sRev.get(), sDly.get() })
        addChildComponent (k);

    //---------------- ADVANCED ----------------
    {   // fila 0
        Panel eq; eq.title = "EQ"; eq.row = 0;
        eq.items = { addKnob ("lowCut", "LOW CUT", " Hz", 0), addKnob ("eqLow", "LOW", " dB", 1),
                     addKnob ("eqMid", "MID", " dB", 1), addKnob ("eqHigh", "HIGH", " dB", 1) };
        eq.spans = { 1, 1, 1, 1 };
        panels.push_back (eq);

        Panel dyn; dyn.title = "DYNAMICS"; dyn.row = 0;
        dyn.items = { addKnob ("comp", "COMP", "%", 0), addCombo ("compMode", "MODE"),
                      addKnob ("deess", "DE-ESS", "%", 0), addKnob ("deessFocus", "FOCUS", " Hz", 0) };
        dyn.spans = { 1, 1.3f, 1, 1 };
        panels.push_back (dyn);

        Panel air; air.title = "AIR"; air.row = 0;
        air.items = { addKnob ("midAir", "MID AIR", "%", 0), addKnob ("highAir", "HIGH AIR", "%", 0) };
        air.spans = { 1, 1 };
        panels.push_back (air);
    }
    {   // fila 1
        Panel tune; tune.title = "TUNE"; tune.row = 1;
        tuneCell = new TuneCell (proc.apvts);
        advComps.add (tuneCell);
        addChildComponent (tuneCell);
        tune.items = { tuneCell, addCombo ("key", "KEY"), addCombo ("scale", "SCALE"),
                       addKnob ("tuneSpeed", "SPEED", " ms", 0), addKnob ("tuneAmount", "AMOUNT", "%", 0),
                       addKnob ("tuneTol", "TOLERANCE", " c", 0) };
        tune.spans = { 1.1f, 0.9f, 1.7f, 1, 1, 1 };
        panels.push_back (tune);

        Panel tone; tone.title = "TONE"; tone.row = 1;
        tone.items = { addCombo ("toneType", "TYPE"), addKnob ("toneInt", "INTENSIDAD", "%", 0),
                       addKnob ("magic", "MAGIC", "%", 0) };
        tone.spans = { 1.5f, 1, 1 };
        panels.push_back (tone);
    }
    {   // fila 2
        Panel mic; mic.title = "MIC"; mic.row = 2;
        mic.items = { addCombo ("mic", "MIC"), addKnob ("micBody", "BODY", "%", 0), addKnob ("micAir", "AIR", "%", 0) };
        mic.spans = { 2.2f, 1, 1 };
        panels.push_back (mic);

        Panel rev; rev.title = "REVERB"; rev.row = 2;
        rev.items = { addCombo ("revType", "TYPE"), addKnob ("revMix", "MIX", "%", 0),
                      addKnob ("revLow", "LOW CUT", " Hz", 0), addKnob ("revHigh", "HIGH CUT", " Hz", 0) };
        rev.spans = { 1.2f, 1, 1, 1 };
        panels.push_back (rev);

        Panel dly; dly.title = "DELAY"; dly.row = 2;
        dly.items = { addCombo ("dlyTime", "TIME"), addKnob ("dlyMix", "MIX", "%", 0), addKnob ("dlyFb", "FEEDBACK", "%", 0),
                      addKnob ("dlyLow", "LOW CUT", " Hz", 0), addKnob ("dlyHigh", "HIGH CUT", " Hz", 0) };
        dly.spans = { 1, 1, 1, 1, 1 };
        panels.push_back (dly);
    }

    const bool startAdv = (bool) proc.apvts.state.getProperty ("uiAdvanced", false);
    (startAdv ? advBtn : simpleBtn).setToggleState (true, juce::dontSendNotification);
    setAdvanced (startAdv);

    startTimerHz (15);
}

VocodexEditor::~VocodexEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocodexEditor::setAdvanced (bool adv)
{
    advanced = adv;
    proc.apvts.state.setProperty ("uiAdvanced", adv, nullptr);

    for (auto* k : { sComp.get(), sMagic.get(), sRev.get(), sDly.get() })
        k->setVisible (! adv);
    for (auto* c : advComps)
        c->setVisible (adv);

    if (adv) setSize (1080, 570);
    else     setSize (720, 430);
    resized();
    repaint();
}

void VocodexEditor::rebuildPresetButtons()
{
    presetBtns.clear();
    const auto& list = getPresetList();
    const juce::String current = proc.apvts.state.getProperty ("presetName", "").toString();

    for (int i = 0; i < (int) list.size(); ++i)
    {
        if (list[(size_t) i].category != selectedCat) continue;
        auto* b = new juce::TextButton (juce::String::fromUTF8 (list[(size_t) i].name));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (3);
        b->onClick = [this, i] { proc.applyPreset (i); };
        if (current == juce::String::fromUTF8 (list[(size_t) i].name))
            b->setToggleState (true, juce::dontSendNotification);
        presetBtns.add (b);
        addAndMakeVisible (b);
    }
}

//==============================================================================
void VocodexEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBlack);

    g.setColour (kWhite);
    g.setFont (boldFont (28.0f));
    g.drawText ("VOCODEX PRO", kMargin + 4, 8, 300, 34, juce::Justification::centredLeft, false);
    g.setColour (kGold);
    g.setFont (boldFont (11.0f));
    g.drawText ("VOCAL SUITE  -  v2.4", kMargin + 6, 42, 300, 16, juce::Justification::centredLeft, false);

    g.setColour (kPanelEdge);
    g.drawHorizontalLine (kHeaderH, (float) kMargin, (float) (getWidth() - kMargin));

    if (advanced)
    {
        for (const auto& p : panels)
        {
            const auto r = p.bounds.toFloat();
            g.setColour (kPanel);
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (kPanelEdge);
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
            g.setColour (kWhite);
            g.setFont (boldFont (12.0f));
            g.drawText (p.title, p.bounds.getX() + 12, p.bounds.getY() + 4, p.bounds.getWidth() - 20, 18,
                        juce::Justification::centredLeft, false);
            g.setColour (kGold);
            g.fillRect (p.bounds.getX() + 12, p.bounds.getY() + 23, 22, 2);
        }
    }
}

void VocodexEditor::resized()
{
    const int W = getWidth(), H = getHeight();

    advBtn.setBounds    (W - kMargin - 120, 16, 120, 32);
    simpleBtn.setBounds (W - kMargin - 250, 16, 120, 32);

    int x = kMargin;
    for (auto& b : catBtns) { b.setBounds (x, kHeaderH + 10, 130, 30); x += 136; }

    x = kMargin;
    for (auto* b : presetBtns) { b->setBounds (x, kHeaderH + 46, 150, 28); x += 156; }

    if (! advanced)
    {
        auto area = juce::Rectangle<int> (kMargin, kContentTop + 10, W - 2 * kMargin, H - kContentTop - 24);
        const int cellW = area.getWidth() / 4;
        KnobComp* ks[4] = { sComp.get(), sMagic.get(), sRev.get(), sDly.get() };
        for (int i = 0; i < 4; ++i)
            ks[i]->setBounds (area.getX() + i * cellW + 10, area.getY(), cellW - 20, area.getHeight());
    }
    else
    {
        layoutAdvanced ({ kMargin, kContentTop, W - 2 * kMargin, H - kContentTop - kMargin });
    }
}

void VocodexEditor::layoutAdvanced (juce::Rectangle<int> area)
{
    constexpr int gap = 8, pad = 8, titleH = 28;
    const int rowH = (area.getHeight() - 2 * gap) / 3;

    for (int row = 0; row < 3; ++row)
    {
        float totalSpans = 0.0f;
        int n = 0;
        for (const auto& p : panels)
            if (p.row == row)
            {
                for (float s : p.spans) totalSpans += s;
                ++n;
            }
        if (n == 0) continue;

        const float cellW = ((float) (area.getWidth() - (n - 1) * gap - n * 2 * pad)) / totalSpans;
        const int y = area.getY() + row * (rowH + gap);
        float px = (float) area.getX();

        for (auto& p : panels)
        {
            if (p.row != row) continue;
            float ps = 0.0f;
            for (float s : p.spans) ps += s;
            const int pw = (int) std::lround (ps * cellW) + 2 * pad;
            p.bounds = { (int) std::lround (px), y, pw, rowH };

            float ix = px + (float) pad;
            for (size_t i = 0; i < p.items.size(); ++i)
            {
                const int iw = (int) std::lround (p.spans[i] * cellW);
                p.items[i]->setBounds ((int) std::lround (ix), y + titleH, iw, rowH - titleH - 6);
                ix += p.spans[i] * cellW;
            }
            px += (float) pw + (float) gap;
        }
    }
}

void VocodexEditor::timerCallback()
{
    if (advanced && tuneCell != nullptr)
        tuneCell->setNote (noteText (proc.getDetectedMidi()));
}

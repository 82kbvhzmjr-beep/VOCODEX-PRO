#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"

namespace vxui
{
inline const juce::Colour kBlack { 0xff000000 };
inline const juce::Colour kPanel { 0xff0b0b0b };
inline const juce::Colour kPanelEdge { 0xff2c2c2c };
inline const juce::Colour kWhite { 0xffffffff };
inline const juce::Colour kGold { 0xffd4af37 };

inline juce::Font boldFont (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }
inline juce::Font plainFont (float size) { return juce::Font (juce::FontOptions (size)); }
}

//==============================================================================
class VxLook : public juce::LookAndFeel_V4
{
public:
    VxLook()
    {
        using namespace vxui;
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff111111));
        setColour (juce::ComboBox::textColourId, kWhite);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3a3a));
        setColour (juce::ComboBox::arrowColourId, kGold);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff111111));
        setColour (juce::PopupMenu::textColourId, kWhite);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, kGold);
        setColour (juce::PopupMenu::highlightedTextColourId, kBlack);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151515));
        setColour (juce::TextButton::buttonOnColourId, kGold);
        setColour (juce::TextButton::textColourOffId, kWhite);
        setColour (juce::TextButton::textColourOnId, kBlack);
        setColour (juce::Label::textColourId, kWhite);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace vxui;
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const float r = d * 0.5f;

        g.setColour (juce::Colour (0xff141414));
        g.fillEllipse (c.x - r, c.y - r, d, d);

        juce::Path track;
        track.addCentredArc (c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff333333));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float angle = startAngle + pos * (endAngle - startAngle);
        juce::Path val;
        val.addCentredArc (c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, startAngle, angle, true);
        g.setColour (kGold);
        g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Numero del control en dorado, en el centro
        g.setColour (kGold);
        g.setFont (boldFont (juce::jlimit (9.0f, 16.0f, d * 0.17f)));
        g.drawText (s.getTextFromValue (s.getValue()),
                    juce::Rectangle<float> (c.x - r * 0.8f, c.y - 10.0f, r * 1.6f, 20.0f).toNearestInt(),
                    juce::Justification::centred, false);
    }
};

//==============================================================================
class KnobComp : public juce::Component
{
public:
    KnobComp (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& name,
              const juce::String& suffix = {}, int decimals = 0)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (160);
        addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, vxui::kWhite);
        label.setFont (vxui::boldFont (11.0f));
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
        slider.setNumDecimalPlacesToDisplay (decimals);
        slider.setTextValueSuffix (suffix);
        if (auto* p = apvts.getParameter (id))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromBottom (18));
        slider.setBounds (r);
    }

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
};

//==============================================================================
class ComboComp : public juce::Component
{
public:
    ComboComp (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& name)
    {
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
            box.addItemList (cp->choices, 1);
        addAndMakeVisible (box);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, vxui::kWhite);
        label.setFont (vxui::boldFont (11.0f));
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        attach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, box);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromBottom (18));
        box.setBounds (r.withSizeKeepingCentre (r.getWidth() - 6, 26));
    }

private:
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attach;
};

//==============================================================================
// Boton TUNE ON + nota detectada
class TuneCell : public juce::Component
{
public:
    explicit TuneCell (juce::AudioProcessorValueTreeState& apvts)
    {
        button.setButtonText ("TUNE ON");
        button.setClickingTogglesState (true);
        addAndMakeVisible (button);
        attach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "tuneOn", button);

        caption.setText ("NOTA DETECTADA", juce::dontSendNotification);
        caption.setJustificationType (juce::Justification::centred);
        caption.setFont (vxui::boldFont (9.0f));
        caption.setColour (juce::Label::textColourId, vxui::kWhite);
        addAndMakeVisible (caption);

        note.setText ("--", juce::dontSendNotification);
        note.setJustificationType (juce::Justification::centred);
        note.setFont (vxui::boldFont (16.0f));
        note.setColour (juce::Label::textColourId, vxui::kGold);
        addAndMakeVisible (note);
    }

    void setNote (const juce::String& t)
    {
        if (note.getText() != t) note.setText (t, juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (3, 0);
        button.setBounds (r.removeFromTop (30));
        r.removeFromTop (8);
        caption.setBounds (r.removeFromTop (14));
        note.setBounds (r.removeFromTop (24));
    }

private:
    juce::TextButton button;
    juce::Label caption, note;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attach;
};

//==============================================================================
class VocodexEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VocodexEditor (VocodexProcessor&);
    ~VocodexEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Panel
    {
        juce::String title;
        int row = 0;
        std::vector<juce::Component*> items;
        std::vector<float> spans;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    void setAdvanced (bool adv);
    void rebuildPresetButtons();
    void layoutAdvanced (juce::Rectangle<int> area);

    KnobComp* addKnob (const char* id, const char* name, const char* suffix, int decimals);
    ComboComp* addCombo (const char* id, const char* name);

    VocodexProcessor& proc;
    VxLook look;
    bool advanced = false;
    int selectedCat = 0;

    juce::TextButton simpleBtn, advBtn;
    juce::TextButton catBtns[kNumCategories];
    juce::OwnedArray<juce::TextButton> presetBtns;

    // SIMPLE
    std::unique_ptr<KnobComp> sComp, sMagic, sRev, sDly;

    // ADVANCED
    juce::OwnedArray<juce::Component> advComps;
    std::vector<Panel> panels;
    TuneCell* tuneCell = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocodexEditor)
};

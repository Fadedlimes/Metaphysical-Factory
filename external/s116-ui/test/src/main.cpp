#include <array>
#include <cmath>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>

#include "S116Theme.h"
#include "SkinManager.h"
#include "SilverLookAndFeel.h"
#include "S116Widgets.h"
#include "S116Layout.h"

#include "BinaryData.h"

using namespace S116;

class DummyProcessor : public juce::AudioProcessor
{
public:
    DummyProcessor() : juce::AudioProcessor (BusesProperties()) {}
    const juce::String getName() const override { return "Dummy"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
};

static juce::AudioProcessorValueTreeState::ParameterLayout createParams()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("knob", "Knob", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("mode", "Mode", juce::StringArray { "A", "B", "C" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("toggle", "Toggle", false));
    return { params.begin(), params.end() };
}

//==============================================================================
// Debug-only bounds audit. Checks every parent/child pair:
//  - child bounds inside the parent
//  - no two siblings intersect
//  - a Label's text fits inside its own bounds
static double relativeLuminance (juce::Colour c)
{
    auto linear = [] (float x) { return x <= 0.04045f ? x / 12.92f : std::pow ((x + 0.055f) / 1.055f, 2.4f); };
    return 0.2126 * linear (c.getFloatRed()) + 0.7152 * linear (c.getFloatGreen()) + 0.0722 * linear (c.getFloatBlue());
}

static double contrastRatio (juce::Colour a, juce::Colour b)
{
    const double la = relativeLuminance (a), lb = relativeLuminance (b);
    return (juce::jmax (la, lb) + 0.05) / (juce::jmin (la, lb) + 0.05);
}

static juce::Colour compositeOver (juce::Colour fg, juce::Colour bg, float alpha)
{
    return juce::Colour::fromFloatRGBA (fg.getFloatRed() * alpha + bg.getFloatRed() * (1.0f - alpha),
                                        fg.getFloatGreen() * alpha + bg.getFloatGreen() * (1.0f - alpha),
                                        fg.getFloatBlue() * alpha + bg.getFloatBlue() * (1.0f - alpha),
                                        1.0f);
}

static juce::Font labelFont (const juce::Label& l)
{
    const auto style = l.getProperties().getWithDefault ("style", "caption").toString();
    float size = 13.0f, kern = 0.085f;
    if (style == "field" || style == "sub") { size = 12.0f; kern = 0.10f; }
    else if (style == "accent")
    {
        size = (float) (double) l.getProperties().getWithDefault ("captionSize", 13.0);
        kern = 0.085f;
    }
    return S116::skin().ui (size, kern);
}

static bool auditComponent (juce::Component& parent, const juce::String& labelIn, bool verbose)
{
    bool ok = true;
    const auto& kids = parent.getChildren();
    const auto parentBounds = parent.getLocalBounds();

    if (verbose)
        std::cout << "  " << labelIn << " " << parentBounds.getWidth() << "x" << parentBounds.getHeight() << std::endl;

    for (int i = 0; i < kids.size(); ++i)
    {
        auto* c = kids[i];
        const auto b = c->getBounds();
        const auto name = c->getName().isNotEmpty() ? c->getName() : juce::String ("child ") + juce::String (i);

        if (! parentBounds.contains (b))
        {
            std::cout << "  FAIL " << labelIn << " / " << name << " leaves parent: " << b.toString() << std::endl;
            ok = false;
        }

        if (auto* label = dynamic_cast<juce::Label*> (c))
        {
            const auto tw = S116::textWidth (labelFont (*label), label->getText());
            if (tw > (float) label->getWidth() + 0.5f)
            {
                std::cout << "  FAIL " << labelIn << " / " << name << " text width " << tw
                          << " > cell " << label->getWidth() << " (" << label->getText() << ")" << std::endl;
                ok = false;
            }
        }

        for (int j = i + 1; j < kids.size(); ++j)
        {
            if (b.intersects (kids[j]->getBounds()))
            {
                std::cout << "  FAIL " << labelIn << " / " << name << " overlaps "
                          << (kids[j]->getName().isNotEmpty() ? kids[j]->getName() : juce::String ("child ") + juce::String (j))
                          << " " << b.toString() << " vs " << kids[j]->getBounds().toString() << std::endl;
                ok = false;
            }
        }

        if (verbose)
            std::cout << "    " << name << " " << b.toString() << std::endl;

        if (! auditComponent (*c, name, verbose))
            ok = false;
    }

    return ok;
}

static bool auditFaderTickContrast (bool verbose)
{
    struct Entry { S116::ThemeId id; const char* name; };
    const Entry entries[] = {
        { S116::ThemeId::GraphiteRed, "Graphite" },
        { S116::ThemeId::JP8000Blue, "JP-8000 Blue" },
        { S116::ThemeId::ClassicSilver, "Classic Silver" },
        { S116::ThemeId::SH101Red, "SH-101 Red" },
        { S116::ThemeId::MetaphysicalBlack, "Metaphysical Black" }
    };

    bool ok = true;
    for (const auto& e : entries)
    {
        const auto t = S116::Theme::make (e.id);
        const auto tickTop = compositeOver (t.tick, t.panelTop, 0.9f);
        const auto tickBottom = compositeOver (t.tick, t.panelBottom, 0.9f);
        const auto ct = contrastRatio (tickTop, t.panelTop);
        const auto cb = contrastRatio (tickBottom, t.panelBottom);
        const bool pass = ct >= 3.0 && cb >= 3.0;
        if (verbose)
            std::cout << "TICK CONTRAST " << e.name << ": top " << juce::String (ct, 2)
                      << ":1, bottom " << juce::String (cb, 2) << ":1 "
                      << (pass ? "PASS" : "FAIL") << " (min 3.00:1)" << std::endl;
        ok = ok && pass;
    }
    return ok;
}

static bool auditLedIndicatorContrast (bool verbose)
{
    struct Entry { S116::ThemeId id; const char* name; juce::Colour base; };
    const Entry entries[] = {
        { S116::ThemeId::GraphiteRed, "Graphite", juce::Colour (0xffff3047) },
        { S116::ThemeId::JP8000Blue, "JP-8000 Blue", juce::Colour (0xffff3047) },
        { S116::ThemeId::ClassicSilver, "Classic Silver", juce::Colour (0xffff3047) },
        { S116::ThemeId::SH101Red, "SH-101 Red", juce::Colour (0xffff3047) },
        { S116::ThemeId::MetaphysicalBlack, "Metaphysical Black", juce::Colour (0xffff2a85) }
    };

    bool ok = true;
    for (const auto& e : entries)
    {
        auto t = S116::Theme::make (e.id);
        t.applyLed (e.base);
        const auto ratio = contrastRatio (t.ledOn, t.ledOff);
        const bool pass = ratio >= 3.0;
        if (verbose)
            std::cout << "LED INDICATOR CONTRAST " << e.name << ": on "
                      << t.ledOn.toDisplayString (false) << " vs off "
                      << t.ledOff.toDisplayString (false) << " = "
                      << juce::String (ratio, 2) << ":1 "
                      << (pass ? "PASS" : "FAIL") << " (min 3.00:1)" << std::endl;
        ok = ok && pass;
    }
    return ok;
}

//==============================================================================
class FaderCell : public juce::Component
{
public:
    explicit FaderCell (const juce::String& caption)
    {
        title.setText (caption, juce::dontSendNotification);
        S116::layout::styleCaption (title);
        title.setName ("TITLE");

        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setRange (0.0, 1.0, 0.01);
        fader.setValue (0.55, juce::dontSendNotification);
        fader.getProperties().set ("s116Fader", true);
        fader.setName ("FADER");

        rec.setButtonText ("REC");
        rec.setClickingTogglesState (true);
        rec.setToggleState (true, juce::dontSendNotification);
        rec.getProperties().set ("s116Compact", true);
        rec.setName ("REC");

        speed.setButtonText ("1x");
        speed.getProperties().set ("s116Compact", true);
        speed.setName ("1x");

        addAndMakeVisible (title);
        addAndMakeVisible (fader);
        addAndMakeVisible (rec);
        addAndMakeVisible (speed);
        setName ("FADER CELL " + caption);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        title.setBounds (area.removeFromTop (16));

        auto buttons = area.removeFromBottom (68);
        rec.setBounds (buttons.removeFromTop (34));
        speed.setBounds (buttons);

        area.removeFromBottom (4);
        fader.setBounds (area);
    }

    juce::Label title;
    juce::Slider fader;
    juce::TextButton rec, speed;
};

class BankPanelPreview : public juce::Component
{
public:
    BankPanelPreview() : panel ("BANK A: VOICES & FINE TRIM")
    {
        panel.setCompactPadding (true);
        panel.setSubtitle ("SINE / TRI / BIPOLAR / NOISE");
        panel.setName ("BANK PANEL");
        addAndMakeVisible (panel);

        static const char* labels[] = { "SIN 1", "SIN 2", "SIN 3", "TRI", "PULSE", "LP NOISE" };
        for (int i = 0; i < 6; ++i)
        {
            trims[(size_t) i].setSliderStyle (juce::Slider::RotaryVerticalDrag);
            trims[(size_t) i].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            trims[(size_t) i].setRange (0.0, 1.0, 0.01);
            trims[(size_t) i].setValue (0.5, juce::dontSendNotification);
            trims[(size_t) i].setName ("TRIM " + juce::String (i + 1));
            panel.addAndMakeVisible (trims[(size_t) i]);

            cells[(size_t) i] = std::make_unique<FaderCell> (labels[i]);
            panel.addAndMakeVisible (*cells[(size_t) i]);
        }

        setSize (426, 296);
    }

    void resized() override
    {
        panel.setBounds (getLocalBounds());
        auto a = panel.getContentArea();

        const int cellW = 68, cellGap = 2;
        const int cellTotal = 6 * cellW + 5 * cellGap;
        const int cellX = a.getX() + (a.getWidth() - cellTotal) / 2;
        const int cellY = a.getY() + 64;
        const int cellH = a.getHeight() - 64;

        for (int i = 0; i < 6; ++i)
        {
            const int x = cellX + i * (cellW + cellGap);
            S116::layout::placeBareKnob (trims[(size_t) i], x, a.getY(), cellW, 42);
            cells[(size_t) i]->setBounds (x, cellY, cellW, cellH);
        }
    }

    bool auditLayout (bool verbose) const
    {
        bool ok = auditComponent (const_cast<SectionPanel&> (panel), "BANK", verbose);
        const auto tf = S116::skin().ui (16.0f, 0.15f);
        const auto sf = S116::skin().ui (12.0f, 0.10f);
        const auto tw = S116::textWidth (tf, "BANK A: VOICES & FINE TRIM");
        const auto sw = S116::textWidth (sf, "SINE / TRI / BIPOLAR / NOISE");
        if (tw + sw + 24.0f > (float) panel.getWidth() - 12.0f)
        {
            std::cout << "  FAIL BANK title+subtitle do not fit: " << tw + sw + 24.0f
                      << " > " << panel.getWidth() - 12 << std::endl;
            ok = false;
        }

        for (int i = 0; i < 6; ++i)
        {
            const int knobCentre = trims[(size_t) i].getBounds().getCentreX();
            const int cellCentre = cells[(size_t) i]->getBounds().getCentreX();
            const bool aligned = knobCentre == cellCentre;
            if (verbose)
                std::cout << "  KNOB ALIGN " << (i + 1) << ": " << knobCentre
                          << " == " << cellCentre << " " << (aligned ? "PASS" : "FAIL") << std::endl;
            ok = ok && aligned;
        }

        return ok;
    }

    SectionPanel panel;
    std::array<juce::Slider, 6> trims;
    std::array<std::unique_ptr<FaderCell>, 6> cells;
};

static int upperRowHeight (int windowH)
{
    const int body = windowH - 84 - 24;
    return (int) ((float) (body - 10) * 0.485f);
}

static int lowerRowHeight (int windowH)
{
    const int body = windowH - 84 - 24;
    return body - 10 - upperRowHeight (windowH);
}

class MainComponent : public juce::Component
{
public:
    MainComponent (juce::AudioProcessorValueTreeState& apvts)
        : panel ("STANDARD PANEL"),
          compactPanel ("COMPACT PANEL"),
          knob (apvts, "knob", "KNOB"),
          bigKnob (apvts, "knob", "BIG", KnobCell::Options { KnobCell::Size::Big }),
          momentary ("MOM", false),
          toggle ("TOG", true),
          combo (apvts, "mode", "COMPACT PANEL MODE"),
          lcd ()
    {
        lcd.setLines ({ "LCD TEST", "1.00" });
        compactPanel.setCompactPadding (true);
        compactPanel.setSubtitle ("24 / 4 / 4 + SUBTITLE");

        faderTitle.setText ("MOTION", juce::dontSendNotification);
        S116::layout::styleCaption (faderTitle);
        bareCaption.setText ("BARE KNOB", juce::dontSendNotification);
        S116::layout::styleCaption (bareCaption);
        accentText.setText ("ROOT: A2", juce::dontSendNotification);
        S116::layout::styleAccent (accentText);
        compactNote.setText ("COMPACT BUTTONS / 26 PX FACE", juce::dontSendNotification);
        S116::layout::styleField (compactNote);
        compactNote.setJustificationType (juce::Justification::centredLeft);

        buttonStatesLabel.setText ("BUTTON STATES (OFF / ON)", juce::dontSendNotification);
        S116::layout::styleField (buttonStatesLabel);
        buttonStatesLabel.setJustificationType (juce::Justification::centredLeft);
        buttonStatesLabel.setName ("BUTTON STATES");

        auto setupStateButton = [] (juce::TextButton& b, const juce::String& text,
                                    bool on, const juce::String& name)
        {
            b.setButtonText (text);
            b.setClickingTogglesState (true);
            b.setToggleState (on, juce::dontSendNotification);
            b.getProperties().set ("s116Compact", true);
            b.setName (name);
        };
        setupStateButton (recOff,   "REC", false, "REC OFF");
        setupStateButton (recOn,    "REC", true,  "REC ON");
        setupStateButton (speedOff, "1x",  false, "1x OFF");
        setupStateButton (speedOn,  "1x",  true,  "1x ON");

        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setRange (0.0, 1.0, 0.01);
        fader.setValue (0.62, juce::dontSendNotification);
        fader.getProperties().set ("s116Fader", true);

        bareKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        bareKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        bareKnob.setRange (0.0, 1.0, 0.01);
        bareKnob.setValue (0.4, juce::dontSendNotification);
        bareKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                      juce::MathConstants<float>::pi * 2.8f, true);

        compactRec.setButtonText ("REC");
        compactRec.setClickingTogglesState (true);
        compactRec.setToggleState (true, juce::dontSendNotification);
        compactRec.getProperties().set ("s116Compact", true);

        compactSpeed.setButtonText ("1x");
        compactSpeed.getProperties().set ("s116Compact", true);

        addAndMakeVisible (chassis);
        addAndMakeVisible (header);
        addAndMakeVisible (panel);
        addAndMakeVisible (compactPanel);

        panel.setName ("STANDARD PANEL");
        compactPanel.setName ("COMPACT PANEL");
        panel.addAndMakeVisible (knob);
        panel.addAndMakeVisible (bigKnob);
        panel.addAndMakeVisible (momentary);
        panel.addAndMakeVisible (toggle);
        panel.addAndMakeVisible (combo);
        panel.addAndMakeVisible (lcd);
        panel.addAndMakeVisible (scope);
        compactPanel.addAndMakeVisible (faderTitle);
        compactPanel.addAndMakeVisible (fader);
        compactPanel.addAndMakeVisible (compactRec);
        compactPanel.addAndMakeVisible (compactSpeed);
        compactPanel.addAndMakeVisible (bareKnob);
        compactPanel.addAndMakeVisible (bareCaption);
        compactPanel.addAndMakeVisible (accentText);
        compactPanel.addAndMakeVisible (compactNote);
        compactPanel.addAndMakeVisible (buttonStatesLabel);
        compactPanel.addAndMakeVisible (recOff);
        compactPanel.addAndMakeVisible (recOn);
        compactPanel.addAndMakeVisible (speedOff);
        compactPanel.addAndMakeVisible (speedOn);

        knob.setName ("KNOB"); bigKnob.setName ("BIG"); momentary.setName ("MOM");
        toggle.setName ("TOG"); combo.setName ("MODE"); lcd.setName ("LCD");
        scope.setName ("SCOPE"); fader.setName ("FADER"); faderTitle.setName ("MOTION");
        compactRec.setName ("REC"); compactSpeed.setName ("1x");
        bareKnob.setName ("BARE KNOB"); bareCaption.setName ("BARE CAPTION");
        accentText.setName ("ROOT ACCENT"); compactNote.setName ("COMPACT NOTE");
        buttonStatesLabel.setName ("BUTTON STATES");

        scope.setLabels ("TEST CRT // RED", "ZOOM 1.00x");
        setSize (1280, 820);
    }

    void resized() override
    {
        chassis.setBounds (getLocalBounds());
        header.setBounds (36, 14, getWidth() - 72, 72);
        panel.setBounds (36, 100, 600, getHeight() - 136);
        compactPanel.setBounds (660, 100, getWidth() - 696, getHeight() - 136);

        auto a = panel.getContentArea();
        knob.setBounds (a.getX(), a.getY(), 120, knob.getIdealHeight());
        bigKnob.setBounds (a.getX() + 140, a.getY(), 140, bigKnob.getIdealHeight());
        momentary.setBounds (buttonBoundsFromFace ({ a.getX() + 300, a.getY() + 10, 100, 34 }));
        toggle.setBounds (buttonBoundsFromFace ({ a.getX() + 300, a.getY() + 60, 100, 34 }));
        combo.setBounds (a.getX() + 300, a.getY() + 120, 180, 52);
        lcd.setBounds (a.getX() + 300, a.getY() + 190, 180, 60);
        scope.setBounds (a.getX() + 300, a.getY() + 270, 200, 200);

        auto c = compactPanel.getContentArea();
        faderTitle.setBounds (c.getX() + 40, c.getY() + 20, 54, 16);
        fader.setBounds (c.getX() + 40, c.getY() + 40, 54, 240);
        compactRec.setBounds (c.getX() + 40, c.getY() + 288, 54, 34);
        compactSpeed.setBounds (c.getX() + 40, c.getY() + 326, 54, 34);

        S116::layout::placeBareKnob (bareKnob, c.getX() + 140, c.getY() + 40, 64, 42);
        bareCaption.setBounds (c.getX() + 120, c.getY() + 110, 104, 16);
        accentText.setBounds (c.getX() + 240, c.getY() + 40, 220, 16);
        compactNote.setBounds (c.getX() + 240, c.getY() + 88, 280, 16);

        buttonStatesLabel.setBounds (c.getX() + 40, c.getY() + 400, 300, 16);
        const int stateW = 54, stateGap = 6, stateY = c.getY() + 420;
        recOff.setBounds   (c.getX() + 40 + 0 * (stateW + stateGap), stateY, stateW, 34);
        recOn.setBounds    (c.getX() + 40 + 1 * (stateW + stateGap), stateY, stateW, 34);
        speedOff.setBounds (c.getX() + 40 + 2 * (stateW + stateGap), stateY, stateW, 34);
        speedOn.setBounds  (c.getX() + 40 + 3 * (stateW + stateGap), stateY, stateW, 34);
    }

    bool auditLayout (bool verbose)
    {
        bool ok = auditComponent (panel, "STANDARD", verbose);
        ok = auditComponent (compactPanel, "COMPACT", verbose) && ok;
        return ok;
    }

private:
    ChassisBackground chassis;
    HeaderBar header;
    SectionPanel panel;
    SectionPanel compactPanel;
    KnobCell knob;
    KnobCell bigKnob;
    LedButton momentary;
    LedButton toggle;
    LabelledCombo combo;
    LcdText lcd;
    CrtScopeFrame scope;
    juce::Slider fader;
    juce::Label faderTitle;
    juce::TextButton compactRec, compactSpeed;
    juce::Slider bareKnob;
    juce::Label bareCaption;
    juce::Label accentText;
    juce::Label compactNote;
    juce::Label buttonStatesLabel;
    juce::TextButton recOff, recOn, speedOff, speedOn;
};

class PreviewWindow : public juce::DocumentWindow
{
public:
    explicit PreviewWindow (const juce::String& title)
        : juce::DocumentWindow (title, juce::Colours::black, juce::DocumentWindow::allButtons) {}

    void closeButtonPressed() override
    {
        juce::MessageManager::getInstance()->stopDispatchLoop();
    }
};

int main (int argc, char** argv)
{
    bool preview = false, bank = false;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        if (arg == "--preview") preview = true;
        if (arg == "--bank")    bank = true;
    }

    juce::ScopedJuceInitialiser_GUI init;

    auto barlow = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf,
                                                           BinaryData::BarlowCondensedBold_ttfSize);
    auto mono = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf,
                                                         BinaryData::ShareTechMonoRegular_ttfSize);
    S116::skin().setTypefaces (barlow, mono);

    SilverLookAndFeel laf;
    S116::skin().setTheme (ThemeId::MetaphysicalBlack);
    S116::skin().setLedColour (juce::Colour (0xffff2a85));
    laf.applyTheme();

    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts (processor, nullptr, "PARAMS", createParams());

    if (bank)
    {
        struct Size { int h; const char* name; };
        const Size sizes[] = {
            { upperRowHeight (730), "1280x730 Bank A" },
            { lowerRowHeight (730), "1280x730 Bank B" },
            { upperRowHeight (820), "1280x820 Bank A" },
            { lowerRowHeight (820), "1280x820 Bank B" }
        };

        bool ok = true;
        for (const auto& size : sizes)
        {
            BankPanelPreview bankPreview;
            bankPreview.setSize (426, size.h);
            std::cout << "BANK " << size.name << " 426x" << size.h << std::endl;
            ok = bankPreview.auditLayout (true) && ok;
        }
        ok = auditFaderTickContrast (true) && ok;
        ok = auditLedIndicatorContrast (true) && ok;
        std::cout << (ok ? "BANK AUDIT PASS" : "BANK AUDIT FAIL") << std::endl;
        if (!ok)
            return 1;

        BankPanelPreview bankPreview;
        bankPreview.setLookAndFeel (&laf);
        bankPreview.setSize (426, upperRowHeight (820));

        PreviewWindow window ("S-116 Bank preview 426x340");
        window.setContentNonOwned (&bankPreview, true);
        window.setContentComponentSize (426, upperRowHeight (820));
        const int borderH = window.getHeight() - upperRowHeight (820);
        window.setResizable (true, true);
        window.setResizeLimits (window.getWidth(), upperRowHeight (730) + borderH,
                                window.getWidth(), lowerRowHeight (820) + borderH);
        window.setVisible (true);
        window.toFront (true);
        std::cout << "Bank preview. Resize height to 296..362; close the window to exit." << std::endl;
        juce::MessageManager::getInstance()->runDispatchLoop();
        window.setVisible (false);
        return 0;
    }

    MainComponent comp (apvts);
    comp.setLookAndFeel (&laf);
    comp.sendLookAndFeelChange();

    PreviewWindow window ("s116-ui v1.4 preview");
    window.setContentNonOwned (&comp, true);
    window.setContentComponentSize (1280, 820);
    window.setVisible (true);
    window.toFront (true);

    std::cout << "S-116 theme: " << S116::skin().theme().name
              << " / LED " << S116::skin().theme().ledOn.toDisplayString (false)
              << " / panel " << S116::skin().theme().panelTop.toDisplayString (false)
              << std::endl;

    const bool layoutOk = comp.auditLayout (true);
    std::cout << (layoutOk ? "PREVIEW LAYOUT PASS" : "PREVIEW LAYOUT FAIL") << std::endl;

    if (preview)
    {
        std::cout << "Preview mode. Close the window to exit." << std::endl;
        juce::MessageManager::getInstance()->runDispatchLoop();
        window.setVisible (false);
        return 0;
    }

    // Non-interactive smoke test: paint once, then exit.
    juce::Timer::callAfterDelay (500, [] { juce::MessageManager::getInstance()->stopDispatchLoop(); });
    juce::MessageManager::getInstance()->runDispatchLoop();
    window.setVisible (false);
    return layoutOk ? 0 : 1;
}

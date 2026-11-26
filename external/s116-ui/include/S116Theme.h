#pragma once
// ============================================================================
//  S116Theme.h   S-116 UI kit
//  Colour tokens for the four chassis variants (Graphite, JP-8000 Blue, Classic
//  Silver, SH-101 Red) plus metrics shared by the LookAndFeel and the widgets.
//  Sizes are "design pixels": the editor lays out on a fixed canvas (a plugin uses
//  1920 x 1500) and scales it to the window with Component::setTransform.
// ============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

namespace S116
{
    // ---- shared metrics (design px) ---------------------------------------
    constexpr int   kDesignW = 1920, kDesignH = 1500;

    constexpr float kTickMargin   = 11.0f;  // halo around a knob body that holds the tick marks
    constexpr float kGlyphMargin  = 34.0f;  // halo for waveform-glyph ("morph") knobs and their aligned neighbours

    // Buttons: the *face* is 34px tall. The Component bounds must be the face
    // grown by (3 left, 3 right, 3 top, 6 bottom) so the black ring + 3px ledge
    // + outline fit inside the component (JUCE can't paint outside its bounds).
    constexpr int   kButtonFaceH  = 34;
    constexpr int   kBtnInsL = 3, kBtnInsR = 3, kBtnInsT = 3, kBtnInsB = 6;
    constexpr int   kSelectH      = 32;     // dropdown ("sel") height incl. its 1px outline
    constexpr int   kReadoutH     = 20;     // LED value plate under knobs

    inline juce::Rectangle<int> buttonBoundsFromFace (juce::Rectangle<int> face)
    {
        return { face.getX() - kBtnInsL, face.getY() - kBtnInsT,
                 face.getWidth() + kBtnInsL + kBtnInsR,
                 face.getHeight() + kBtnInsT + kBtnInsB };
    }

    // numThemes remains the count of the four cycled themes so existing consumers
    // (notably Simple116) keep their theme-cycle behaviour. MetaphysicalBlack is an
    // additional fixed theme selected explicitly with skin().setTheme(...).
    enum class ThemeId { GraphiteRed = 0, JP8000Blue, ClassicSilver, SH101Red, numThemes, MetaphysicalBlack = numThemes };

    struct Theme
    {
        juce::String name;

        // chassis + rack ears
        juce::Colour chassisTop, chassisBody, chassisBottom;          // vertical gradient, stops at 0 / 5% / 96% / 100%
        juce::Colour cheekA, cheekB, cheekC, holeDark, holeRing;      // side cheeks (gradient stops 0 / 30% / 100%)

        // header bar + logo
        juce::Colour hdrTop, hdrBottom, logo, logoAccent;

        // section panels + text
        juce::Colour panelTop, panelBottom, scanline;
        juce::Colour heading;    // section titles + ".lab" field labels
        juce::Colour sub;        // sub-headings + row labels
        juce::Colour caption;    // control captions under knobs
        juce::Colour rule;       // faint separators (light line; a 1px #0b0c0e line sits below it)
        juce::Colour ruleDark { 0xff0b0c0e };

        // knobs
        juce::Colour knobBodyTop, knobBodyBottom, knobRing, tick, capA, capB, pointer, pointerGlow;
        float        capHighlight = 0.12f;                            // alpha of the 145deg white sheen on the cap
        bool         pointerFollowsLed = false;

        // buttons
        juce::Colour btnTop, btnBottom, btnLedge, btnRing, btnText, btnTextShadow;
        float        btnTextShadowDy = -1.0f;

        // dropdowns ("sel")
        juce::Colour selTop, selBottom, selOutline, selText, selArrow;

        // LED / LCD family (defaults are red; applyLed() re-derives them from the user's LED colour)
        juce::Colour ledText      { 0xffff5a66 };   // LCD text
        juce::Colour ledReadout   { 0xffff6b76 };   // value plates under knobs
        juce::Colour ledOn        { 0xffff3047 };   // button LED lit
        juce::Colour ledOff       { 0xff3a0d12 };   // button LED unlit
        juce::Colour lcdTop       { 0xff3a0d12 }, lcdBottom { 0xff1a0508 };
        juce::Colour plateTop     { 0xff0e0506 }, plateBottom { 0xff2a0a0e };

        // scope bezel
        juce::Colour scopeBezelTop, scopeBezelBottom;

        static juce::Colour c (juce::uint32 argb) { return juce::Colour (argb); }

        void applyLed (juce::Colour base)
        {
            const float h = base.getHue();
            const float k = juce::jmin (1.0f, base.getSaturation() / 0.8f);
            auto hsv = [h, k] (float sat, float val) { return juce::Colour::fromHSV (h, sat * k, val, 1.0f); };
            ledOn       = hsv (0.81f, 1.00f);
            ledText     = hsv (0.65f, 1.00f);
            ledReadout  = hsv (0.58f, 1.00f);
            ledOff      = hsv (0.78f, 0.23f);
            lcdTop      = ledOff;
            lcdBottom   = hsv (0.85f, 0.10f);
            plateTop    = hsv (0.70f, 0.055f);
            plateBottom = hsv (0.76f, 0.165f);
            if (pointerFollowsLed) { pointer = ledOn; pointerGlow = ledOn.withAlpha (0.8f); }
        }

        static Theme make (ThemeId id)
        {
            Theme t;
            // ---- shared "dark LCD" dropdown + black text shadow defaults -----
            t.selTop = c (0xff0e0f12); t.selBottom = c (0xff25282e);
            t.selOutline = c (0xff4a4f59); t.selText = c (0xffffd0d4); t.selArrow = c (0xffff5a66);
            t.btnTextShadow = juce::Colours::black;
            t.pointerGlow = c (0xffff2d45).withAlpha (0.8f);
            t.scopeBezelTop = c (0xff4b4f58); t.scopeBezelBottom = c (0xff17181c);
            t.rule = juce::Colours::white.withAlpha (0.08f);
            t.scanline = juce::Colours::white.withAlpha (0.025f);

            switch (id)
            {
            case ThemeId::GraphiteRed:   // "Graphite · Red LED"  (default / Main.dc.html)
                t.pointerFollowsLed = true;
                t.name = "Graphite";
                t.chassisTop = c (0xff34373e); t.chassisBody = c (0xff1b1d22); t.chassisBottom = c (0xff101114);
                t.cheekA = c (0xff3b3e45); t.cheekB = c (0xff1c1e22); t.cheekC = c (0xff16181b);
                t.holeDark = c (0xff030304); t.holeRing = c (0xff6e737c);
                t.hdrTop = c (0xff2d3037); t.hdrBottom = c (0xff16181c);
                t.logo = c (0xffd8dce3); t.logoAccent = c (0xffff3b4e);
                t.panelTop = c (0xff2b2e35); t.panelBottom = c (0xff1f2227);
                t.heading = c (0xffaab1bd); t.sub = c (0xff9da4b0); t.caption = c (0xffc3c8d1);
                t.knobBodyTop = c (0xff464b54); t.knobBodyBottom = c (0xff14161a);
                t.knobRing = c (0xff3a3e46); t.tick = c (0xff7d8591);
                t.capA = c (0xff4b4f58); t.capB = c (0xff2e3138);
                t.pointer = c (0xffff2d45);
                t.btnTop = c (0xff4b4f58); t.btnBottom = c (0xff2e3138); t.btnLedge = c (0xff14161a);
                t.btnRing = c (0xff3a3e46); t.btnText = c (0xffc3c8d1);
                break;

            case ThemeId::JP8000Blue:    // "JP-8000 · Metallic Blue"
                t.pointerFollowsLed = true;
                t.name = "JP-8000 Blue";
                t.chassisTop = c (0xff3a5a92); t.chassisBody = c (0xff1a2d50); t.chassisBottom = c (0xff0c1830);
                t.cheekA = c (0xff3d5f9a); t.cheekB = c (0xff1a2c4d); t.cheekC = c (0xff101b32);
                t.holeDark = c (0xff030304); t.holeRing = c (0xff6e737c);
                t.hdrTop = c (0xff2c4a80); t.hdrBottom = c (0xff14233f);
                t.logo = c (0xfff1f5fc); t.logoAccent = c (0xffff7b4a);
                t.panelTop = c (0xff2f4f86); t.panelBottom = c (0xff223e6c);
                t.heading = c (0xffdce7f8); t.sub = c (0xffb4c7e6); t.caption = c (0xffeaf1fb);
                t.knobBodyTop = c (0xff464b54); t.knobBodyBottom = c (0xff14161a);
                t.knobRing = c (0xff7f97c2); t.tick = c (0xffd6e2f6);
                t.capA = c (0xff58627a); t.capB = c (0xff363d52);
                t.pointer = c (0xffff2d45);
                t.btnTop = c (0xff58627a); t.btnBottom = c (0xff363d52); t.btnLedge = c (0xff14161a);
                t.btnRing = c (0xff7f97c2); t.btnText = c (0xffeaf1fb);
                t.selTop = c (0xff080e1a); t.selBottom = c (0xff1a2640); t.selOutline = c (0xff8aa2cc);
                t.scopeBezelTop = c (0xff58627a); t.scopeBezelBottom = c (0xff0f1522);
                break;

            case ThemeId::ClassicSilver: // "Classic Silver" (dark header, light panels, dark knob pointers)
                t.name = "Classic Silver";
                t.chassisTop = c (0xffb9bec7); t.chassisBody = c (0xffa4aab4); t.chassisBottom = c (0xff8c929c);
                t.cheekA = c (0xffd3d7de); t.cheekB = c (0xffa3a9b3); t.cheekC = c (0xff8b919b);
                t.holeDark = c (0xff25272c); t.holeRing = c (0xffeef0f3);
                t.hdrTop = c (0xff363b44); t.hdrBottom = c (0xff1b1e24);
                t.logo = c (0xffd8dce3); t.logoAccent = c (0xffff3b4e);
                t.panelTop = c (0xffdfe2e8); t.panelBottom = c (0xffc4c9d1);
                t.scanline = juce::Colours::white.withAlpha (0.30f);
                t.heading = c (0xff272c34); t.sub = c (0xff4a5260); t.caption = c (0xff14171a);
                t.rule = juce::Colours::black.withAlpha (0.18f);
                t.knobBodyTop = c (0xff9aa0ab); t.knobBodyBottom = c (0xff4a4f59);
                t.knobRing = juce::Colours::white.withAlpha (0.5f); t.tick = c (0xff2b3038);
                t.capA = c (0xffe9ecf1); t.capB = c (0xffc3c8d1); t.capHighlight = 0.6f;
                t.pointer = c (0xff14171a); t.pointerGlow = juce::Colours::white.withAlpha (0.4f);
                t.btnTop = c (0xffe9ecf1); t.btnBottom = c (0xffc3c8d1); t.btnLedge = c (0xff6b717c);
                t.btnRing = juce::Colours::white.withAlpha (0.5f); t.btnText = c (0xff14171a);
                t.btnTextShadow = juce::Colours::white.withAlpha (0.7f); t.btnTextShadowDy = 1.0f;
                break;

            case ThemeId::MetaphysicalBlack: // matte black + hot pink identity
                t.name = "Metaphysical Black";
                t.chassisTop = c (0xff14161a); t.chassisBody = c (0xff0a0b0d); t.chassisBottom = c (0xff050608);
                t.cheekA = c (0xff1a1c21); t.cheekB = c (0xff101216); t.cheekC = c (0xff050608);
                t.holeDark = c (0xff030304); t.holeRing = c (0xff4a4f59);
                t.hdrTop = c (0xff16181d); t.hdrBottom = c (0xff0a0b0d);
                t.logo = c (0xffe6e9f0); t.logoAccent = c (0xffff2a85);
                t.panelTop = c (0xff111216); t.panelBottom = c (0xff0a0b0d);
                t.scanline = juce::Colours::white.withAlpha (0.02f);
                t.heading = c (0xffe6e9f0); t.sub = c (0xffb8bfcc); t.caption = c (0xffc3c8d1);
                t.rule = juce::Colours::white.withAlpha (0.10f);
                t.knobBodyTop = c (0xff2a2d33); t.knobBodyBottom = c (0xff0c0d10);
                t.knobRing = c (0xff3a3e46); t.tick = c (0xff8a919e);
                t.capA = c (0xff343841); t.capB = c (0xff1b1d22); t.capHighlight = 0.12f;
                t.pointer = c (0xffff2a85); t.pointerFollowsLed = false;
                t.pointerGlow = c (0xffff2a85).withAlpha (0.8f);
                t.btnTop = c (0xff262a31); t.btnBottom = c (0xff15171b); t.btnLedge = c (0xff050608);
                t.btnRing = c (0xff3a3e46); t.btnText = c (0xffe6e9f0);
                t.selTop = c (0xff0e0f12); t.selBottom = c (0xff1b1e24); t.selOutline = c (0xff4a4f59);
                t.selText = c (0xffffd0d4); t.selArrow = c (0xffff2a85);
                t.scopeBezelTop = c (0xff2a2d33); t.scopeBezelBottom = c (0xff0a0b0d);
                break;

            case ThemeId::SH101Red:      // "SH-101 · Red"
            default:
                t.name = "SH-101 Red";
                t.chassisTop = c (0xffc62732); t.chassisBody = c (0xffa81c27); t.chassisBottom = c (0xff7e121b);
                t.cheekA = c (0xff5a1018); t.cheekB = c (0xff3d0a10); t.cheekC = c (0xff2a070b);
                t.holeDark = c (0xff030304); t.holeRing = c (0xffd8a5a9);
                t.hdrTop = c (0xff8f1822); t.hdrBottom = c (0xff5e0f16);
                t.logo = c (0xfff5e9e9); t.logoAccent = c (0xffffb3b8);
                t.panelTop = c (0xffc0252f); t.panelBottom = c (0xffa51d27);
                t.heading = c (0xfffff4f4); t.sub = c (0xffffffff); t.caption = c (0xfffff4f4);
                t.rule = juce::Colours::white.withAlpha (0.20f);
                t.knobBodyTop = c (0xff2b2b2e); t.knobBodyBottom = c (0xff0a0a0b);
                t.knobRing = c (0xff7b141c); t.tick = c (0xffffd6d9);
                t.capA = c (0xff2a2a2d); t.capB = c (0xff17171a);
                t.pointer = c (0xffffffff); t.pointerGlow = juce::Colours::white.withAlpha (0.45f);
                t.btnTop = c (0xff2a2a2d); t.btnBottom = c (0xff17171a); t.btnLedge = c (0xff0a0a0b);
                t.btnRing = c (0xff7b141c); t.btnText = c (0xfffff4f4);
                t.selOutline = c (0xff8f1822);
                t.scopeBezelTop = c (0xff2a2a2d); t.scopeBezelBottom = c (0xff17181c);
                break;
            }
            return t;
        }
    };
}

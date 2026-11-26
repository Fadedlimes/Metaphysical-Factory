#pragma once
// ============================================================================
//  S116Widgets.h   S-116 UI kit
//  Skinned building blocks. Static artwork (panels, chassis, LCD plates, CRT
//  bezel) is rasterised once into a 2x Image and re-used; only dynamic content
//  (LCD text, graphs, scope trace) is painted live.
//
//    ChassisBackground  full-window background + rack-ear cheeks
//    HeaderBar          top bar with S-116 logo
//    SectionPanel       titled panel with screws + engraved title rules
//    StyledText         captions / field labels / row labels / sub-headings
//    LcdPlate, LcdText, EnvelopeLcd, StepBarsLcd
//    CrtScopeFrame      bezel + grid + labels; put your OscilloscopeComponent
//                       inside getScreenBounds() and let it draw the trace only
//    KnobCell           knob + LED readout + caption, bound to an APVTS param
//    LabelledCombo      dropdown bound to an AudioParameterChoice
//    LedButton          ledge button (toggle w/ optional APVTS attachment)
// ============================================================================
#include <array>
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include "SkinManager.h"

namespace S116
{
    // ------------------------------------------------------------------------
    //  Base: caches paintSkin() output as a 2x raster; rebuilds on resize/theme.
    // ------------------------------------------------------------------------
    class SkinnedComponent : public juce::Component, private juce::ChangeListener
    {
    public:
        SkinnedComponent()           { skin().addChangeListener (this); setOpaque (false); }
        ~SkinnedComponent() override { skin().removeChangeListener (this); }

        void invalidateCache() { cache = {}; repaint(); }

        void invalidateSkin() { cache = {}; repaint(); }

        void paint (juce::Graphics& g) override
        {
            const int w = getWidth(), h = getHeight();
            if (w <= 0 || h <= 0) return;
            constexpr int S = 2;
            if (! cache.isValid() || cache.getWidth() != w * S || cache.getHeight() != h * S)
            {
                cache = juce::Image (juce::Image::ARGB, w * S, h * S, true);
                juce::Graphics ig (cache);
                ig.addTransform (juce::AffineTransform::scale ((float) S));
                paintSkin (ig, getLocalBounds().toFloat(), skin().theme());
            }
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (cache, getLocalBounds().toFloat());
            paintDynamic (g);
        }

    protected:
        virtual void paintSkin (juce::Graphics&, juce::Rectangle<float> bounds, const Theme&) = 0;
        virtual void paintDynamic (juce::Graphics&) {}

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override { cache = {}; repaint(); }
        juce::Image cache;
    };

    // ---- small drawing helpers ------------------------------------------------
    inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float d = 11.0f)
    {
        const juce::Rectangle<float> r = juce::Rectangle<float> (d, d).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.fillEllipse (r.translated (0.0f, 1.0f));
        juce::ColourGradient grad (juce::Colour (0xffd6d9de), r.getX() + d * 0.35f, r.getY() + d * 0.30f,
                                   juce::Colour (0xff2a2d32), r.getX() + d * 0.35f + d * 0.75f, r.getY() + d * 0.30f, true);
        grad.addColour (0.6, juce::Colour (0xff6b7078));
        g.setGradientFill (grad);
        g.fillEllipse (r);
    }

    inline void glowText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r,
                          juce::Colour col, juce::Justification j, float glowAlpha = 0.18f)
    {
        g.setColour (col.withMultipliedAlpha (glowAlpha));
        for (auto o : { juce::Point<float> (-1, 0), { 1, 0 }, { 0, -1 }, { 0, 1 } })
            g.drawText (s, r.translated (o.x, o.y), j, false);
        g.setColour (col);
        g.drawText (s, r, j, false);
    }

    // ------------------------------------------------------------------------
    class ChassisBackground : public SkinnedComponent
    {
    protected:
        void paintSkin (juce::Graphics& g, juce::Rectangle<float> b, const Theme& t) override
        {
            const float w = b.getWidth(), h = b.getHeight();
            juce::ColourGradient grad (t.chassisTop, 0, 0, t.chassisBottom, 0, h, false);
            grad.addColour (0.05, t.chassisBody);
            grad.addColour (0.96, t.chassisBody);
            g.setGradientFill (grad);
            g.fillRect (b);

            // rack-ear cheeks, 32px wide, holes every 150px starting at y = 75
            for (float x0 : { 0.0f, w - 32.0f })
            {
                const juce::Rectangle<int> cheek ((int) x0, 0, 32, (int) h);
                juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 10, { 0, 0 }).drawForRectangle (g, cheek);

                juce::ColourGradient cg (t.cheekA, x0, 0, t.cheekC, x0 + 32.0f, 0, false);
                cg.addColour (0.30, t.cheekB);
                g.setGradientFill (cg);
                g.fillRect (cheek);
                g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillRect (x0, 0.0f, 2.0f, h);
                g.setColour (juce::Colours::black.withAlpha (0.70f)); g.fillRect (x0 + 30.0f, 0.0f, 2.0f, h);

                for (float y = 75.0f; y < h; y += 150.0f)
                {
                    const juce::Point<float> c (x0 + 16.0f, y);
                    g.setColour (t.holeRing); g.fillEllipse (juce::Rectangle<float> (13.6f, 13.6f).withCentre (c));
                    g.setColour (t.holeDark); g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (c));
                }
            }
        }
    };

    // ------------------------------------------------------------------------
    class HeaderBar : public SkinnedComponent
    {
    public:
        /** x where the logo starts (design px). */
        static constexpr int kLogoLeft = 24;

        /** Logo drawn as three pieces in sequence: prefix (logo colour), accent (accent colour with a glow),
            suffix (logo colour). Default is "S", "-", "116". Example: setLogo ("S", "-", "303"). */
        void setLogo (const juce::String& prefix, const juce::String& accent, const juce::String& suffix)
        {
            logoPrefix = prefix; logoAccentText = accent; logoSuffix = suffix;
            invalidateSkin();
        }

        /** x of the logo's right edge (design px): start laying out header controls at this + 14. */
        float getLogoRight() const
        {
            const auto f = skin().ui (40.0f, 0.10f);
            return (float) kLogoLeft + textWidth (f, logoPrefix) + textWidth (f, logoAccentText) + textWidth (f, logoSuffix) + 6.0f;
        }

    protected:
        void paintSkin (juce::Graphics& g, juce::Rectangle<float> b, const Theme& t) override
        {
            g.setGradientFill (juce::ColourGradient::vertical (t.hdrTop, 0, t.hdrBottom, b.getHeight()));
            g.fillRoundedRectangle (b, 6.0f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRect (6.0f, 0.0f, b.getWidth() - 12.0f, 1.0f);

            const auto f = skin().ui (40.0f, 0.10f);                // 40px, letter-spacing 4px
            g.setFont (f);
            float x = (float) kLogoLeft;
            auto piece = [&] (const juce::String& s, juce::Colour col, bool glow)
            {
                const float tw = textWidth (f, s) + 2.0f;
                const juce::Rectangle<float> r (x, 0.0f, tw + 6.0f, b.getHeight());
                g.setColour (juce::Colours::black);                   g.drawText (s, r.translated (0, -1), juce::Justification::centredLeft, false);
                g.setColour (juce::Colours::white.withAlpha (0.2f));  g.drawText (s, r.translated (0, 1),  juce::Justification::centredLeft, false);
                if (glow) glowText (g, s, r, col, juce::Justification::centredLeft, 0.2f);
                else      { g.setColour (col); g.drawText (s, r, juce::Justification::centredLeft, false); }
                x += tw;
            };
            piece (logoPrefix, t.logo, false);
            piece (logoAccentText, t.logoAccent, true);
            piece (logoSuffix, t.logo, false);
        }

    private:
        juce::String logoPrefix { "S" }, logoAccentText { "-" }, logoSuffix { "116" };
    };

    // ------------------------------------------------------------------------
    class SectionPanel : public SkinnedComponent
    {
    public:
        explicit SectionPanel (const juce::String& titleText) : title (titleText) {}

        /** Enables the dense panel mode: 24 top / 4 sides / 4 bottom, left-aligned
            title and an optional right-aligned subtitle. Default is the design
            padding 46 / 22 / 20 so existing layouts are unchanged. */
        void setCompactPadding (bool compact)
        {
            if (compactPadding != compact) { compactPadding = compact; invalidateSkin(); }
        }

        void setSubtitle (const juce::String& s)
        {
            if (subtitle != s) { subtitle = s; invalidateSkin(); }
        }

        /** Panel padding: 46 top, 22 left/right, 20 bottom by default;
            24 / 4 / 4 in compact mode. */
        juce::Rectangle<int> getContentArea() const
        {
            if (compactPadding)
                return getLocalBounds().withTrimmedTop (24).reduced (4, 0).withTrimmedBottom (4);
            return getLocalBounds().withTrimmedTop (46).reduced (22, 0).withTrimmedBottom (20);
        }

    protected:
        void paintSkin (juce::Graphics& g, juce::Rectangle<float> b, const Theme& t) override
        {
            const float w = b.getWidth(), h = b.getHeight();
            juce::Path pp; pp.addRoundedRectangle (b, 6.0f);

            g.setGradientFill (juce::ColourGradient::vertical (t.panelTop, 0, t.panelBottom, h));
            g.fillPath (pp);
            {
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (pp);
                g.setColour (t.scanline);                                    // 1px line every 3px
                for (float y = h - 1.0f; y >= 0.0f; y -= 3.0f) g.fillRect (0.0f, y, w, 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.14f)); g.fillRect (0.0f, 0.0f, w, 1.0f);
                g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::transparentBlack, h - 4.0f,
                                                                   juce::Colours::black.withAlpha (0.5f), h));
                g.fillRect (0.0f, h - 4.0f, w, 4.0f);
            }

            const auto f = skin().ui (16.0f, 0.15f);
            const float tw = textWidth (f, title) + 2.0f;

            if (compactPadding)
            {
                // compact title band: left title, optional right subtitle, no screws
                const auto sf = skin().ui (12.0f, 0.10f);
                const float sw = subtitle.isNotEmpty() ? textWidth (sf, subtitle) : 0.0f;
                const bool showSub = subtitle.isNotEmpty() && (tw + sw + 24.0f <= w - 12.0f);
                const juce::Rectangle<float> tr (6.0f, 2.0f, w - 12.0f, 18.0f);

                g.setColour (t.rule);
                g.fillRect (4.0f, 22.0f, w - 8.0f, 1.0f);

                g.setFont (f);
                g.setColour (juce::Colours::black);
                g.drawText (title, tr.translated (0, -1), juce::Justification::centredLeft, false);
                g.setColour (t.heading);
                g.drawText (title, tr, juce::Justification::centredLeft, false);

                if (showSub)
                {
                    g.setFont (sf);
                    const juce::Rectangle<float> sr (w - 6.0f - sw - 2.0f, 2.0f, sw + 2.0f, 18.0f);
                    g.setColour (juce::Colours::black);
                    g.drawText (subtitle, sr.translated (0, -1), juce::Justification::centredRight, false);
                    g.setColour (t.sub);
                    g.drawText (subtitle, sr, juce::Justification::centredRight, false);
                }
                return;
            }

            // engraved title: ---- TITLE ----
            const float left = 34.0f, right = w - 34.0f, cy = 12.0f + 9.5f;
            const float lineW = juce::jmax (0.0f, (right - left - tw - 24.0f) * 0.5f);
            for (float x0 : { left, right - lineW })
            {
                g.setColour (t.ruleDark);                     g.fillRect (x0, cy - 1.0f, lineW, 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillRect (x0, cy, lineW, 1.0f);
            }
            const juce::Rectangle<float> tr (left + lineW + 12.0f, cy - 10.0f, tw + 4.0f, 20.0f);
            g.setFont (f);
            g.setColour (juce::Colours::black); g.drawText (title, tr.translated (0, -1), juce::Justification::centred, false);
            g.setColour (t.heading);            g.drawText (title, tr, juce::Justification::centred, false);

            drawScrew (g, { 12.0f + 5.5f, 13.0f + 5.5f });
            drawScrew (g, { w - 12.0f - 5.5f, 13.0f + 5.5f });
        }

    private:
        juce::String title, subtitle;
        bool compactPadding = false;
    };

    // ------------------------------------------------------------------------
    /** Captions, field labels, row labels and sub-headings. Reads the theme at paint time. */
    class StyledText : public juce::Component
    {
    public:
        enum class Style { Caption, Field, RowLabel, SubHeading, Accent };

        StyledText (const juce::String& s, Style st, float heightOverride = 0.0f)
            : text (s), style (st), fontH (heightOverride)
        { setInterceptsMouseClicks (false, false); }

        void setText (const juce::String& s) { text = s; repaint(); }
        void setJustification (juce::Justification j) { just = j; }

        void paint (juce::Graphics& g) override
        {
            const auto& t = skin().theme();
            auto r = getLocalBounds().toFloat();
            switch (style)
            {
                case Style::Caption:    g.setFont (skin().ui (fontH > 0 ? fontH : 13.0f, 0.085f)); break;   // 13px / 1.1px
                case Style::Field:      g.setFont (skin().ui (fontH > 0 ? fontH : 12.0f, 0.10f));  break;
                case Style::RowLabel:   g.setFont (skin().ui (fontH > 0 ? fontH : 13.0f, 0.11f));  break;
                case Style::SubHeading: g.setFont (skin().ui (fontH > 0 ? fontH : 12.0f, 0.15f));  break;   // 12px / 1.8px
                case Style::Accent:     g.setFont (skin().ui (fontH > 0 ? fontH : 13.0f, 0.085f)); break;
            }
            auto col = t.sub;
            if (style == Style::Caption)      col = t.caption;
            else if (style == Style::Field)   col = t.heading;
            else if (style == Style::Accent)  col = t.logoAccent;

            if (style == Style::SubHeading)
            {
                g.setColour (t.rule);     g.fillRect (r.getX(), r.getBottom() - 2.0f, r.getWidth(), 1.0f);
                g.setColour (t.ruleDark); g.fillRect (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);
                r = r.withTrimmedBottom (3.0f);
                g.setColour (col); g.drawText (text, r, juce::Justification::centredLeft, false);
                return;
            }
            g.setColour (juce::Colours::black); g.drawText (text, r.translated (0, -1), just, false);
            g.setColour (col);                  g.drawText (text, r, just, false);
        }

    private:
        juce::String text; Style style; float fontH;
        juce::Justification just { juce::Justification::centred };
    };

    // ------------------------------------------------------------------------
    //  LCD family
    // ------------------------------------------------------------------------
    class LcdPlate : public SkinnedComponent
    {
    protected:
        void paintSkin (juce::Graphics& g, juce::Rectangle<float> b, const Theme& t) override
        {
            g.setGradientFill (juce::ColourGradient (t.lcdTop, b.getCentreX(), b.getCentreY(),
                                                     t.lcdBottom, b.getRight(), b.getBottom(), true));
            g.fillRoundedRectangle (b, 4.0f);
            g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::black.withAlpha (0.8f), 0.0f,
                                                               juce::Colours::transparentBlack, 8.0f));
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.fillRect (3.0f, b.getBottom() - 1.0f, b.getWidth() - 6.0f, 1.0f);
        }
    };

    /** Multi-line LCD text (header "EDIT: DLY MIX / 1.00" display etc.). */
    class LcdText : public LcdPlate
    {
    public:
        void setLines (juce::StringArray s, float fontHeight = 13.0f) { lines = std::move (s); fh = fontHeight; repaint(); }
    protected:
        void paintDynamic (juce::Graphics& g) override
        {
            g.setFont (skin().mono (fh));
            const auto r = getLocalBounds().toFloat().reduced (10.0f, 3.0f);
            const float lh = r.getHeight() / (float) juce::jmax (1, lines.size());
            for (int i = 0; i < lines.size(); ++i)
                glowText (g, lines[i], { r.getX(), r.getY() + lh * (float) i, r.getWidth(), lh },
                          skin().theme().ledText, juce::Justification::centredLeft);
        }
    private:
        juce::StringArray lines; float fh = 13.0f;
    };

    /** ADSR LCD: grid + filled envelope. Feed normalised 0..1 values (times as share of width). */
    class EnvelopeLcd : public LcdPlate
    {
    public:
        void setEnvelope (float a, float d, float s, float r) { A = a; D = d; S = s; R = r; repaint(); }
        void setCaption (juce::String s) { cap = std::move (s); repaint(); }
    protected:
        void paintDynamic (juce::Graphics& g) override
        {
            const auto& t = skin().theme();
            auto b = getLocalBounds().toFloat().reduced (8.0f, 6.0f);
            g.setColour (skin().theme().ledText.withAlpha (0.15f));
            for (int i = 1; i < 4; ++i)
            {
                g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * (float) i / 4.0f), b.getX(), b.getRight());
                g.drawVerticalLine   ((int) (b.getX() + b.getWidth()  * (float) i / 4.0f), b.getY(), b.getBottom());
            }
            const float sum = juce::jmax (0.001f, A + D + 0.25f + R);
            const float xA = b.getX() + b.getWidth() * (A / sum), xD = xA + b.getWidth() * (D / sum);
            const float xS = xD + b.getWidth() * (0.25f / sum),   xR = b.getRight();
            const float yTop = b.getY() + 4.0f, yBot = b.getBottom() - 2.0f, yS = yBot - (yBot - yTop) * S;
            juce::Path p;
            p.startNewSubPath (b.getX(), yBot); p.lineTo (xA, yTop); p.lineTo (xD, yS); p.lineTo (xS, yS); p.lineTo (xR, yBot);
            auto fill = p; fill.closeSubPath();
            g.setColour (skin().theme().ledReadout.withAlpha (0.28f)); g.fillPath (fill);
            g.setColour (skin().theme().ledReadout.withAlpha (0.3f)); g.strokePath (p, juce::PathStrokeType (5.0f));
            g.setColour (t.ledReadout);                  g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (cap.isNotEmpty())
            {
                g.setFont (skin().mono (11.0f));
                g.setColour (t.ledText);
                g.drawText (cap, getLocalBounds().toFloat().reduced (10.0f, 4.0f), juce::Justification::topLeft, false);
            }
        }
    private:
        float A = 0.2f, D = 0.3f, S = 0.6f, R = 0.3f; juce::String cap;
    };

    /** 16-bar step display with step numbers + header strip text. */
    class StepBarsLcd : public LcdPlate
    {
    public:
        void setSteps (const std::array<float, 16>& v, int playhead = -1) { steps = v; head = playhead; repaint(); }
        void setHeader (juce::String l, juce::String c, juce::String r) { hl = std::move (l); hc = std::move (c); hr = std::move (r); repaint(); }
    protected:
        void paintDynamic (juce::Graphics& g) override
        {
            const auto& t = skin().theme();
            auto b = getLocalBounds().toFloat().reduced (10.0f, 6.0f);
            g.setFont (skin().mono (11.0f)); g.setColour (t.ledText);
            auto hdr = b.removeFromTop (14.0f);
            g.drawText (hl, hdr, juce::Justification::centredLeft, false);
            g.drawText (hc, hdr, juce::Justification::centred, false);
            g.drawText (hr, hdr, juce::Justification::centredRight, false);
            auto nums = b.removeFromBottom (12.0f);
            g.setFont (skin().mono (9.0f));
            const float cw = b.getWidth() / 16.0f;
            for (int i = 0; i < 16; ++i)
            {
                const auto col = juce::Rectangle<float> (b.getX() + cw * (float) i, b.getY(), cw, b.getHeight()).reduced (2.0f, 0.0f);
                const float bh = juce::jmax (2.0f, col.getHeight() * juce::jlimit (0.0f, 1.0f, steps[(size_t) i]));
                const auto bar = col.withTop (col.getBottom() - bh);
                juce::Colour lit = (i == head) ? skin().theme().ledReadout.brighter (0.3f) : skin().theme().ledReadout;
                g.setColour (lit.withAlpha (0.25f)); g.fillRoundedRectangle (bar.expanded (2.0f), 3.0f);
                g.setGradientFill (juce::ColourGradient::vertical (lit, bar.getY(), skin().theme().ledReadout.darker (0.5f), bar.getBottom()));
                g.fillRoundedRectangle (bar, 2.0f);
                g.setColour (skin().theme().ledReadout.darker (0.3f));
                g.drawText (juce::String (i + 1), juce::Rectangle<float> (col.getX(), nums.getY(), col.getWidth(), nums.getHeight()), juce::Justification::centred, false);
            }
        }
    private:
        std::array<float, 16> steps {}; int head = -1; juce::String hl, hc, hr;
    };

    // ------------------------------------------------------------------------
    //  CRT scope frame: bezel + phosphor grid + labels
    // ------------------------------------------------------------------------
    class CrtScopeFrame : public SkinnedComponent
    {
    public:
        juce::Rectangle<int> getScreenBounds() const { return getLocalBounds().reduced (12); }
        void setLabels (juce::String left, juce::String right) { l = std::move (left); r = std::move (right); repaint(); }
        juce::String getLeftLabel() const { return l; }
        juce::String getRightLabel() const { return r; }
        void setPhosphor (juce::Colour c) { if (c != phosphor) { phosphor = c; invalidateSkin(); } }
        juce::Colour getPhosphor() const { return phosphor; }

    protected:
        void paintSkin (juce::Graphics& g, juce::Rectangle<float> b, const Theme& t) override
        {
            g.setGradientFill (juce::ColourGradient (t.scopeBezelTop, 0, 0, t.scopeBezelBottom, b.getWidth(), b.getHeight(), false));
            g.fillRoundedRectangle (b, 14.0f);
            g.setColour (juce::Colours::white.withAlpha (0.18f)); g.fillRect (10.0f, 0.0f, b.getWidth() - 20.0f, 1.0f);

            const auto s = b.reduced (12.0f);
            juce::Path sp; sp.addRoundedRectangle (s, 20.0f);
            g.setGradientFill (juce::ColourGradient (phosphor.withMultipliedBrightness (0.18f), s.getCentreX(), s.getY() + s.getHeight() * 0.4f,
                                                     phosphor.withMultipliedBrightness (0.05f), s.getRight(), s.getBottom(), true));
            g.fillPath (sp);
            {
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (sp);
                g.setColour (phosphor.withAlpha (0.18f));
                for (int i = 1; i < 4; ++i)
                {
                    g.drawHorizontalLine ((int) (s.getY() + s.getHeight() * (float) i / 4.0f), s.getX(), s.getRight());
                    g.drawVerticalLine   ((int) (s.getX() + s.getWidth()  * (float) i / 4.0f), s.getY(), s.getBottom());
                }
                juce::ColourGradient vig (juce::Colours::transparentBlack, s.getCentreX(), s.getCentreY(),
                                          juce::Colours::black.withAlpha (0.85f), s.getX(), s.getY(), true);
                vig.addColour (0.55, juce::Colours::transparentBlack);
                g.setGradientFill (vig); g.fillPath (sp);
            }
        }

        void paintDynamic (juce::Graphics& g) override
        {
            const auto s = getLocalBounds().toFloat().reduced (12.0f);
            g.setFont (skin().mono (11.0f));
            const auto row = s.reduced (16.0f, 0.0f).withHeight (24.0f).translated (0, 4);
            glowText (g, l, row, phosphor, juce::Justification::centredLeft);
            glowText (g, r, row, phosphor, juce::Justification::centredRight);
        }

    private:
        juce::String l { "S-116 CRT // RED" }, r { "ZOOM 1.00x" };
        juce::Colour phosphor { 0xffff3a4c };
    };

    // ------------------------------------------------------------------------
    //  Controls
    // ------------------------------------------------------------------------
    using APVTS = juce::AudioProcessorValueTreeState;

    /** Knob + LED readout + caption. All sizes are design px. */
    class KnobCell : public juce::Component
    {
    public:
        enum class Size { Small = 42, Normal = 52, Big = 76, Aux = 60 };      // body diameters

        struct Options
        {
            Size size = Size::Normal;
            bool morph = false;             // 5-stop waveform glyph knob
            bool glyphRow = false;          // non-morph knob that shares a row with a morph knob (same halo -> aligned bodies)
            int  readoutMinWidth = 54;
        };

        KnobCell (APVTS& apvts, const juce::String& paramID, const juce::String& captionText)
            : KnobCell (apvts, paramID, captionText, Options {}, true) {}

        KnobCell (APVTS& apvts, const juce::String& paramID, const juce::String& captionText, Options o)
            : KnobCell (apvts, paramID, captionText, o, true) {}

        KnobCell (APVTS& apvts, const juce::String& paramID, const juce::String& captionText, Options o, bool autoAttach)
            : opts (o), caption (captionText, StyledText::Style::Caption, o.size == Size::Aux ? 16.0f : 0.0f)
        {
            slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
            slider.getProperties().set ("morph", o.morph);
            slider.getProperties().set ("halo", haloPx());
            addAndMakeVisible (slider);

            readout.setJustificationType (juce::Justification::centred);
            readout.getProperties().set ("readout", true);
            readout.getProperties().set ("big", o.size == Size::Aux);
            readout.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (readout);
            addAndMakeVisible (caption);

            if (autoAttach)
                attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);
            slider.onValueChange = [this] { updateReadout(); };
            updateReadout();
        }

        void bindToChoice (APVTS& apvts, const juce::String& paramID)
        {
            attachment.reset();
            choiceParamID = paramID;
            choiceParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID));
            if (choiceParam != nullptr)
            {
                slider.setRange (0.0, (double) juce::jmax (1, choiceParam->choices.size() - 1), 1.0);
                slider.setNumDecimalPlacesToDisplay (0);
                slider.setValue ((double) choiceParam->getIndex(), juce::dontSendNotification);
                slider.onValueChange = [this, &apvts, paramID] {
                    if (auto* p = apvts.getParameter (paramID))
                        p->setValueNotifyingHost (p->convertTo0to1 ((float) slider.getValue()));
                };
            }
            updateReadout();
        }

        void setHeaderMode (bool hm)
        {
            headerMode = hm;
            caption.setVisible (! hm);
            resized();
        }

        void setValueTextFormat (std::function<juce::String(double)> f)
        {
            valueFormat = std::move (f);
            updateReadout();
        }

        void rebind (APVTS& apvts, const juce::String& paramID)
        {
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);
            updateReadout();
        }

        int haloPx()  const { return (opts.morph || opts.glyphRow) ? (int) kGlyphMargin : (int) kTickMargin; }
        int bodyPx()  const { return (int) opts.size; }
        int squarePx() const { return bodyPx() + 2 * haloPx(); }
        int readoutW() const { return opts.size == Size::Aux ? juce::jmax (68, opts.readoutMinWidth) : opts.readoutMinWidth; }
        int readoutH() const { return opts.size == Size::Aux ? 28 : kReadoutH; }
        int readoutGap() const { return haloPx() > (int) kTickMargin ? 18 : 13; }      // body bottom -> readout top
        int getIdealWidth()  const { return juce::jmax (squarePx(), readoutW()); }
        int getIdealHeight() const { return squarePx() / 2 + bodyPx() / 2 + readoutGap() + readoutH() + 5 + 16; }

        void resized() override
        {
            if (headerMode)
            {
                const int T = squarePx();
                const int rH = readoutH();
                slider.setBounds (0, (getHeight() - T) / 2, T, T);
                readout.setBounds (T + 10, (getHeight() - rH) / 2, readoutW(), rH);
                caption.setBounds (0, 0, 0, 0);
                return;
            }
            const int w = getWidth(), T = squarePx();
            slider.setBounds ((w - T) / 2, 0, T, T);
            const int y = T / 2 + bodyPx() / 2 + readoutGap();
            readout.setBounds ((w - readoutW()) / 2, y, readoutW(), readoutH());
            caption.setBounds (0, readout.getBottom() + 5, w, 16);
        }

        juce::Slider slider;

    private:
        void updateReadout()
        {
            juce::String txt = valueFormat ? valueFormat (slider.getValue()) : slider.getTextFromValue (slider.getValue());
            readout.setText (txt, juce::dontSendNotification);
        }

        Options opts;
        juce::Label readout;
        StyledText caption;
        bool headerMode = false;
        juce::AudioParameterChoice* choiceParam = nullptr;
        juce::String choiceParamID;
        std::function<juce::String(double)> valueFormat;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;   // last: destroyed first
    };

    /** Dropdown bound to an AudioParameterChoice. labelPos: Above (".fld" style), Below (caption), None. */
    class LabelledCombo : public juce::Component
    {
    public:
        enum class LabelPos { None, Above, Below };

        LabelledCombo (APVTS& apvts, const juce::String& paramID, const juce::String& labelText = {}, LabelPos pos = LabelPos::Above)
            : label (labelText, pos == LabelPos::Below ? StyledText::Style::Caption : StyledText::Style::Field), labelPos (pos)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID)))
                combo.addItemList (p->choices, 1);
            addAndMakeVisible (combo);
            if (pos != LabelPos::None) addAndMakeVisible (label);
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramID, combo);
        }

        static int idealHeight (LabelPos p) { return kSelectH + (p == LabelPos::None ? 0 : 6 + 14); }

        void resized() override
        {
            auto r = getLocalBounds();
            if (labelPos == LabelPos::Above)      { label.setBounds (r.removeFromTop (14)); r.removeFromTop (6); }
            else if (labelPos == LabelPos::Below) { label.setBounds (r.removeFromBottom (16)); r.removeFromBottom (4); }
            combo.setBounds (r.removeFromTop (kSelectH));
        }

        juce::ComboBox combo;
        StyledText label;

    private:
        LabelPos labelPos;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    /** Ledge button. Use buttonBoundsFromFace() to size it. toggles=true gives the lit-LED latching style. */
    class LedButton : public juce::TextButton
    {
    public:
        explicit LedButton (const juce::String& buttonText, bool toggles = false) : juce::TextButton (buttonText)
        { setClickingTogglesState (toggles); }

        void attach (APVTS& apvts, const juce::String& paramID)
        {
            setClickingTogglesState (true);
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramID, *this);
        }
    private:
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
}

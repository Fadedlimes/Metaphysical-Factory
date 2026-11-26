#pragma once
// ============================================================================
//  SkinManager.h   S-116 UI kit
//
//  S116::SkinManager (singleton, ChangeBroadcaster): current chassis theme, LED base
//  colour (setLedColour / refreshLed), typefaces (setTypefaces, ui, mono) and the
//  cached knob art (getKnobBase). Listeners (SkinnedComponent, your editor) are told
//  when the theme or the LED colour changes.
// ============================================================================
#include "S116Theme.h"
#include <map>
#include <tuple>
#include <string>

namespace S116
{
    // Version-safe string width (JUCE 7 and 8)
    inline float textWidth (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    class SkinManager : public juce::ChangeBroadcaster
    {
    public:
        static SkinManager& get() { static SkinManager inst; return inst; }

        // ---- theme -------------------------------------------------------
        const Theme& theme() const noexcept { return current; }
        ThemeId themeId() const noexcept    { return id; }

        void setTheme (ThemeId t)
        {

            id = t;
            current = Theme::make (t);
            knobCache.clear();
            refreshLed();                  // keeps the current LED colour across chassis changes
        }

        void setLedColour (juce::Colour base)
        {
            ledBaseColour = base;
            refreshLed();
        }

        void refreshLed()
        {
            current.applyLed (ledBaseColour);
            sendChangeMessage();            // editor listens -> laf.applyTheme(); sendLookAndFeelChange();
        }

        juce::Colour getLedBaseColour() const { return ledBaseColour; }

        /** Releases the raster/typeface caches while keeping the singleton alive.
            Call only when the last editor using this skin has been destroyed. */
        void clearResources()
        {
            knobCache.clear();
            uiTypeface = nullptr;
            monoTypeface = nullptr;
        }

        void cycleTheme()
        {
            setTheme (static_cast<ThemeId> ((static_cast<int> (id) + 1) % static_cast<int> (ThemeId::numThemes)));
        }

        // ---- fonts -------------------------------------------------------
        void setTypefaces (juce::Typeface::Ptr uiFace, juce::Typeface::Ptr monoFace)
        {
            uiTypeface = uiFace; monoTypeface = monoFace;
        }

        // ---- text size (accessibility) -------------------------------------
        /** Multiplies every UI and mono font height (captions, labels, buttons, readouts, LCD text, popups).
            Range 0.9 to 1.3: up to 1.3 the text still fits the standard plate and button sizes. Listeners
            (SkinnedComponent, your editor) are notified; call laf.applyTheme() and sendLookAndFeelChange() in the editor. */
        void setTextScale (float scale)
        {
            scale = juce::jlimit (0.9f, 1.3f, scale);
            if (std::abs (scale - textScale) < 0.001f) return;
            textScale = scale;
            sendChangeMessage();
        }

        float getTextScale() const noexcept { return textScale; }

        /** UI font (condensed bold). kerning is a fraction of height: CSS letter-spacing 1.4px @14px = 0.1 */
        juce::Font ui (float height, float kerning = 0.0f) const
        {
            return make (uiTypeface, height * textScale, true, juce::Font::getDefaultSansSerifFontName()).withExtraKerningFactor (kerning);
        }

        juce::Font mono (float height) const
        {
            return make (monoTypeface, height * textScale, false, juce::Font::getDefaultMonospacedFontName());
        }

        // ---- cached knob art -----------------------------------------------
        /** Component square = body + 2*halo. halo = kTickMargin (11) normally, kGlyphMargin (34) for morph rows. */
        juce::Image getKnobBase (int bodyPx, bool morph, int haloPx)
        {
            const auto key = std::make_tuple (bodyPx, morph, haloPx);
            auto it = knobCache.find (key);
            if (it != knobCache.end()) return it->second;
            auto img = renderKnobBase (bodyPx, morph, haloPx);
            knobCache[key] = img;
            return img;
        }

    private:
        SkinManager() : current (Theme::make (ThemeId::GraphiteRed)) { current.applyLed (ledBaseColour); }

        static juce::Font make (const juce::Typeface::Ptr& tf, float h, bool bold, const juce::String& fallback)
        {
           #if JUCE_MAJOR_VERSION >= 8
            if (tf != nullptr) return juce::Font (juce::FontOptions (tf).withHeight (h));
            return juce::Font (juce::FontOptions (fallback, h, bold ? juce::Font::bold : juce::Font::plain));
           #else
            juce::Font f = (tf != nullptr) ? juce::Font (tf) : juce::Font (fallback, h, bold ? juce::Font::bold : juce::Font::plain);
            f.setHeight (h);
            return f;
           #endif
        }

        // Waveform glyphs for the 5-stop morph knob (SVG path data, 18 x 12 box)
        static const char* glyphPath (int i)
        {
            static const char* d[5] = {
                "M1 6C3.5 .5 6 .5 9 6C12 11.5 14.5 11.5 17 6",   // sine      (-135 deg)
                "M1 6L5 1L13 11L17 6",                            // triangle  (-67.5)
                "M1 10L9 2V10L17 2",                              // saw       (0)
                "M1 10V2H9V10H17",                                // square    (+67.5)
                "M1 10H6V2H9V10H17" };                            // pulse     (+135)
            return d[juce::jlimit (0, 4, i)];
        }

        juce::Image renderKnobBase (int body, bool morph, int halo) const
        {
            constexpr int S = 2;                                   // raster scale
            const int T = body + 2 * halo;
            juce::Image img (juce::Image::ARGB, T * S, T * S, true);
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::scale ((float) S));

            const auto& t = current;
            const float r = (float) body * 0.5f;
            const juce::Point<float> c ((float) T * 0.5f, (float) T * 0.5f);
            auto circ = [] (juce::Point<float> p, float rad) { return juce::Rectangle<float> (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f); };

            // 1. ground shadow
            {
                juce::Path p; p.addEllipse (circ (c, r + 3.0f));
                juce::DropShadow (juce::Colours::black.withAlpha (0.75f), 9, { 0, 5 }).drawForPath (g, p);
            }

            // 2. rim: coloured outline (1px) over black ring (2px)
            g.setColour (t.knobRing);                  g.fillEllipse (circ (c, r + 3.0f));
            g.setColour (juce::Colour (0xff060607));   g.fillEllipse (circ (c, r + 2.0f));

            // 3. matte body disc
            g.setGradientFill (juce::ColourGradient::vertical (t.knobBodyTop, c.y - r, t.knobBodyBottom, c.y + r));
            g.fillEllipse (circ (c, r));

            // 4. tick marks: 11 ticks / 27deg (270deg sweep) or 5 ticks / 67.5deg for morph knobs
            {
                const int   n    = morph ? 5 : 11;
                const float step = morph ? 67.5f : 27.0f;
                g.setColour (t.tick.withAlpha (0.9f));
                for (int i = 0; i < n; ++i)
                {
                    const float a = juce::degreesToRadians (-135.0f + step * (float) i);
                    const float s = std::sin (a), co = std::cos (a);
                    g.drawLine (c.x + s * (r + 4.5f),  c.y - co * (r + 4.5f),
                                c.x + s * (r + 11.0f), c.y - co * (r + 11.0f), 1.4f);
                }
            }

            // 5. cap (offset 2px up): machined sweep + soft sheen
            const float inset = std::round ((float) body * 0.095f);       // 52->5, 42->4, 60->6, 76->7
            const juce::Point<float> cc (c.x, c.y - 2.0f);
            const float cr = r - inset;
            {
                juce::Path p; p.addEllipse (circ (cc, cr));
                juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 3, { 0, 2 }).drawForPath (g, p);

                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (p);
                const int segs = 72;                                      // conic-gradient(from 20deg, A, B 25%, A 50%, B 75%, A)
                for (int i = 0; i < segs; ++i)
                {
                    const float d0 = 360.0f * (float) i / (float) segs;
                    const float d1 = d0 + 360.0f / (float) segs + 0.6f;
                    const float m  = std::fmod (d0 + 2.5f - 20.0f + 360.0f, 180.0f);
                    const float w  = 1.0f - std::abs (m - 90.0f) / 90.0f;
                    g.setColour (t.capA.interpolatedWith (t.capB, w));
                    juce::Path wedge;
                    wedge.addPieSegment (circ (cc, cr + 1.0f), juce::degreesToRadians (d0), juce::degreesToRadians (d1), 0.0f);
                    g.fillPath (wedge);
                }
                g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (t.capHighlight),
                                                         cc.x - cr * 0.7f, cc.y - cr * 0.7f,
                                                         juce::Colours::transparentWhite,
                                                         cc.x + cr * 0.1f, cc.y + cr * 0.1f, false));
                g.fillEllipse (circ (cc, cr));
            }
            g.setColour (juce::Colour (0xff0a0b0d)); g.drawEllipse (circ (cc, cr), 1.0f);
            {
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (0, 0, T, (int) (cc.y - cr * 0.3f));
                g.setColour (juce::Colours::white.withAlpha (0.2f));
                g.drawEllipse (circ ({ cc.x, cc.y + 1.0f }, cr - 0.5f), 1.0f);
            }

            // 6. waveform glyphs around a morph knob (radius = body/2 + 24)
            if (morph)
            {
                for (int i = 0; i < 5; ++i)
                {
                    const float a = juce::degreesToRadians (-135.0f + 67.5f * (float) i);
                    const juce::Point<float> p (c.x + std::sin (a) * (r + 24.0f), c.y - std::cos (a) * (r + 24.0f));
                    auto path = juce::Drawable::parseSVGPath (glyphPath (i));
                    path.applyTransform (juce::AffineTransform::translation (p.x - 9.0f, p.y - 6.0f));
                    g.setColour (t.caption);
                    g.strokePath (path, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
            }
            return img;
        }

        Theme   current;
        ThemeId id = ThemeId::GraphiteRed;
        juce::Colour ledBaseColour { 0xffff3047 };
        float textScale = 1.0f;
        juce::Typeface::Ptr uiTypeface, monoTypeface;
        std::map<std::tuple<int, bool, int>, juce::Image> knobCache;
    };

    inline SkinManager& skin() { return SkinManager::get(); }
}


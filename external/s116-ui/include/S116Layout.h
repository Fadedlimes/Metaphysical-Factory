#pragma once
// ============================================================================
//  S116Layout.h  (NEW FILE)   helpers used by S116EditorLayout.inc
//  Geometry only: nothing here creates or owns an existing control.
// ============================================================================
#include "S116Widgets.h"
#include <map>
#include <vector>

namespace S116::layout
{
    // Gap between a knob's body bottom and the top of its readout plate.
    inline int readoutGap (int halo)               { return halo > (int) kTickMargin ? 18 : 13; }
    inline int knobSquare (int body, bool glyphRow) { return body + 2 * (glyphRow ? (int) kGlyphMargin : (int) kTickMargin); }

    /** Height of the slider component: rotary square + readout plate (text box below). */
    inline int knobSliderHeight (int body, bool glyphRow, int readoutH = 20)
    {
        const int halo = glyphRow ? (int) kGlyphMargin : (int) kTickMargin;
        return knobSquare (body, glyphRow) / 2 + body / 2 + readoutGap (halo) + readoutH;
    }

    /** Full cell height: slider + 5 px + 16 px caption. */
    inline int knobCellHeight (int body, bool glyphRow, bool aux = false)
    {
        return knobSliderHeight (body, glyphRow, aux ? 28 : 20) + 5 + 16;
    }

    inline void styleCaption (juce::Label& l, float size = 0.0f)
    {
        l.getProperties().set ("style", "caption");
        if (size > 0.0f) l.getProperties().set ("captionSize", size);
        l.setJustificationType (juce::Justification::centred);
        l.setInterceptsMouseClicks (false, false);
    }

    inline void styleField (juce::Label& l)
    {
        l.getProperties().set ("style", "field");
        l.setJustificationType (juce::Justification::centredLeft);
        l.setInterceptsMouseClicks (false, false);
    }

    inline void styleSub (juce::Label& l)
    {
        l.getProperties().set ("style", "sub");
        l.setJustificationType (juce::Justification::centredLeft);
        l.setInterceptsMouseClicks (false, false);
    }

    inline void styleAccent (juce::Label& l, float size = 0.0f)
    {
        l.getProperties().set ("style", "accent");
        if (size > 0.0f) l.getProperties().set ("captionSize", size);
        l.setJustificationType (juce::Justification::centred);
        l.setInterceptsMouseClicks (false, false);
    }

    /** Positions a bare rotary knob with no text box and no caption.
        The component is exactly the kit square (body + 2 * halo), centred in cellW.
        To align trim knobs with fader columns, pass the fader cell x/width here
        (for example cellW = 68 for a 64 px knob, giving 2 px per side).
        Returns the square size. */
    inline int placeBareKnob (juce::Slider& s, int x, int y, int cellW, int body = 42,
                              bool glyphRow = false, bool morph = false)
    {
        const int halo = glyphRow ? (int) kGlyphMargin : (int) kTickMargin;
        const int T = body + 2 * halo;
        jassert (cellW >= T);
        auto& p = s.getProperties();
        p.set ("body", body);
        p.set ("halo", halo);
        p.set ("morph", morph);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setBounds (x + (cellW - T) / 2, y, T, T);
        return T;
    }

    /** Position one existing knob cell: slider (rotary + its own text box as LED readout) and its caption label.
        Slider must use TextBoxBelow (existing already does). Returns the cell height. */
    inline int placeKnob (juce::Slider& s, juce::Label& caption, int x, int y, int w, int body,
                          bool glyphRow, bool morph = false, bool aux = false)
    {
        const int rw = aux ? 68 : 54, rh = aux ? 28 : 20;
        auto& p = s.getProperties();
        p.set ("body", body);
        p.set ("halo", glyphRow ? (int) kGlyphMargin : (int) kTickMargin);
        p.set ("morph", morph);
        p.set ("readoutW", rw);
        p.set ("readoutH", rh);
        p.set ("bigReadout", aux);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, rw, rh);

        const int sh = knobSliderHeight (body, glyphRow, rh);
        s.setBounds (x, y, w, sh);
        styleCaption (caption, aux ? 16.0f : 0.0f);
        caption.setBounds (x, y + sh + 5, w, 16);
        return sh + 5 + 16;
    }

    /** Ledge button from its FACE rectangle. */
    inline void placeButton (juce::Component& b, int faceX, int faceY, int faceW)
    {
        b.setBounds (buttonBoundsFromFace ({ faceX, faceY, faceW, kButtonFaceH }));
    }

    /** Dropdown with a field label above (label 14 + 6 gap + 32 combo = 52 px). Returns 52. */
    inline int placeField (juce::ComboBox& c, juce::Label& label, int x, int y, int w)
    {
        styleField (label);
        label.setBounds (x, y, w, 14);
        c.setBounds (x, y + 20, w, kSelectH);
        return 52;
    }

    /** Dropdown with its caption BELOW (ARP row style). Returns 52. */
    inline int placeCaptionedCombo (juce::ComboBox& c, juce::Label& caption, int x, int y, int w)
    {
        c.setBounds (x, y, w, kSelectH);
        styleCaption (caption);
        caption.setBounds (x, y + kSelectH + 4, w, 16);
        return 52;
    }

    /** Equal gaps: returns the y offset (from the top of the area) of every block, with a gap above the
        first and below the last (space-evenly). Never less than minGap. */
    inline std::vector<int> distribute (int available, const std::vector<int>& heights, int minGap = 8)
    {
        int sum = 0;
        for (int h : heights) sum += h;
        const int n = (int) heights.size();
        const int gap = juce::jmax (minGap, (available - sum) / (n + 1));
        std::vector<int> ys;
        int y = gap;
        for (int h : heights) { ys.push_back (y); y += h + gap; }
        return ys;
    }

    /** Owns the purely decorative sub-heading components (DCO 1, MIXER / PERFORMANCE, ...). */
    struct DecorSet
    {
        std::map<juce::String, std::unique_ptr<StyledText>> items;

        StyledText& sub (juce::Component& parent, const juce::String& key, const juce::String& text)
        {
            auto& up = items[key];
            if (up == nullptr)
            {
                up = std::make_unique<StyledText> (text, StyledText::Style::SubHeading);
                parent.addAndMakeVisible (*up);
            }
            return *up;
        }
    };
}

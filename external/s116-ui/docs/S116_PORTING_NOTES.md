# S-116 Porting Notes

Practical lessons from porting a dense synth editor (Metaphysical Factory) to the S-116 kit. These are engineering notes, not new API; the design system remains authoritative.

## 1. The knob halo is a tick band, not decoration

`kTickMargin = 11` is not a soft shadow allowance. The kit draws the 11 ticks between radius `r + 4.5` and `r + 11`; at a 64 px component the outer tick sits on the component edge. Do not overlap halos or rely on body-only hit testing to make overlapping controls look acceptable.

Minimum legible rotary is the Small body (42 px), which needs a **64 x 64** square. Below that the body becomes `min(w,h) - 2*halo`; 24 px gives a 2 px body and the draw call returns early at `body < 8`.

## 2. Derive the minimum window from footprints, not from the old layout

Compute each panel's minimum from its contents plus compact padding, then solve for the window:

- Bank A/B: 28 padding + 64 trim row + 202 fader cell = 294.
- Filters: 28 + 80 knob row + 202 fader cell = 310.
- Telemetry: 2 x 128 screens + 6 gap + 28 padding = 290.
- Body = 294 + 10 + 310 = 614; with header 84 and margins 12/16 the raw height is 726. The 48.5% row split needs 729; add 7 px slack -> **736**.
- Width at 1280: 426 + 10 + 342 + 10 + 468 = 1256 content; plus 24 margins = 1280. Do not add horizontal padding on top of that.

This is faster and safer than trying to shrink knobs below the kit minimum.

## 3. Resize-grip margin

JUCE's `ResizableCornerComponent` measured **18 x 18** in this build. A 16 px bottom margin keeps every control clear (the last button ends at `H-20`), though the grip still overlaps the panel background by 2 px. Use an 18 px bottom margin if zero panel-background overlap is required. The right margin can stay 12 px because the last control is horizontally clear.

## 4. Measure fader-cell widths from the real labels

Use the actual Barlow Condensed Bold 13 px + kit kerning, not guesses:

- `DUAL NOISE` = 63.8 px -> 68 px Bank cell.
- `RESO TUNE` = 59.8 px -> 64 px (or wider) Resochord cell.
- `ABSORB` = 42.1 px -> 54 px Effects cell.
- `LP NOISE` = 49.2 px -> fits a 68 px Bank cell.

At 1280, Banks use 68 px cells / 2 px gaps; Filters 54 / 2; Effects 55 / 2 (8 x 55 + 7 x 2 = 454 inside 460); Sampler/Reso 75 / 2. Centre a 64 px trim in the fader cell (`placeBareKnob` with the fader cell width) so trim and fader centre lines match.

## 5. Opt-in properties

- `slider.getProperties().set("s116Fader", true)` enables the vertical kit fader (slot, matte cap, ticks, LED lane). Leave it off for existing linear sliders.
- `button.getProperties().set("s116Compact", true)` enables the 26 px face / 34 px component compact ledge button, minimum 48 px wide.
- `placeBareKnob(slider, cellX, cellY, cellW, body)` positions a no-text-box knob centred in the cell; pass the fader-cell width to keep rows aligned.

These properties are off by default, which is how Simple116 kept its old pitch/mod wheels and buttons unchanged.

## 6. Metaphysical Black is a fixed theme

`ThemeId::MetaphysicalBlack` is intentionally **outside** `ThemeId::numThemes`, so the four legacy themes keep their existing cycle. Select it explicitly with `skin().setTheme(ThemeId::MetaphysicalBlack)` and set the LED base to `0xffff2a85` if the host wants the full pink identity. The theme's logo accent/pointer are pink; caption text is light enough for 4.5:1 on the panel.

## 7. C1 / C2 / C3 order

1. **C1:** typefaces, fixed theme/LED, `SilverLookAndFeel`, one `layout()`, compact panels, all editor-owned rotaries, captions/accents, header and custom nameplate. Keep the visualizers and motion faders on their existing drawing; list the temporary V4 fallbacks.
2. **C2:** MotionSlider only. Set `s116Fader`, kit caption labels, stacked compact REC/1x buttons, theme wireframe/playhead, recording text marker. Keep attachments, lanes, drag handlers and the 60 Hz timer untouched.
3. **C3:** telemetry only. Remove the local OLED LAF, use 64 px scope dials in 72 px wings, cache the static skin with `SkinnedComponent`, repaint only trace/meters/playhead, and restyle the waveform and SMP badge states.

Doing the steps in this order keeps each screenshot review small and makes fallbacks obvious instead of hiding them in one large restyle.

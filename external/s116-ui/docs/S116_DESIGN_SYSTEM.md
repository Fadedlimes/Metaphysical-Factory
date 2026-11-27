# S-116 Design System (JUCE) v1.4

A skeuomorphic hardware-rack look for JUCE plugins: dark matte chassis, machined knobs, ledge buttons with LED strips, LCD dropdowns, red LED readouts. First used in the Simple116 synth. This document lets an LLM agent apply the same look to another plugin without rediscovering the details.

**Source of truth.** The canonical code is the **S-116 UI kit** (section 4), extracted from the finished Simple116 repo. This version of the document was reconciled line by line against kit v1.0.1 (the five headers) and the Simple116 layout file. Where this document and the kit disagree, the kit wins; report the difference.

**Changes from v1.0 of this document:** corrected `sub` text colours (Graphite, SH-101); the button face is a flat colour (not a gradient); the dropdown shipped in Simple116 is the themed `LookAndFeel_V4` combo box (the LCD-plate version is opt-in); envelope and step displays are coloured from `ledReadout`; the BinaryData symbol names; the two ways to build a control (`placeKnob` vs `KnobCell`); the real `SkinManager` API; new Appendix C (synth layout kit) and Appendix D (known issues and kit patch).

**Changes in v1.4:** added the fixed Metaphysical Black theme; opt-in vertical fader; opt-in compact ledge button (26 px face / 34 px component / 48 px minimum width); compact `SectionPanel` padding (24 / 4 / 4) with optional subtitle; `accent` label style; `placeBareKnob`. `ThemeId::numThemes` still counts the four legacy themes so existing theme cycles do not change; select Metaphysical Black explicitly with `skin().setTheme(ThemeId::MetaphysicalBlack)`.

---

## 0. How to use this document (read first)

1. Read sections 1 to 3 to know what the look is.
2. Read section 4 to know what the kit contains.
3. Follow section 6 (the playbook) step by step. Do not skip phases.
4. Section 7 lists the mistakes that cost the most time on the first plugin. Read it before writing any code.
5. Section 8 is the acceptance checklist. A plugin is not "done" until every item has printed evidence.

### Non-negotiable rules

* **Re-skin, do not rewrite.** Keep every existing component, handler, timer, listener, attachment and callback. Change drawing code, colours, fonts and bounds only.
* **Never stub, hide, merge or delete an existing control.** If it does not fit the layout, say so; do not silently drop it.
* **No claim without printed output.** Report raw output of tests, never "should work".
* **Commit after every step** on a branch, and keep the pre-redesign commit reachable.
* **Do not overwrite `slider.onValueChange`** on existing sliders. Chain it or use `addListener`.

---

## 1. Visual identity

| Aspect | Rule |
|---|---|
| Mood | Studio hardware rack: matte graphite, subtle depth, no gloss or specular glare |
| Light | Everything is lit from above: highlights on top edges, shadows below |
| Text | Uppercase condensed labels; monospace LED text for values |
| LEDs | One LED colour per plugin instance (user selectable) drives every LED element |
| Scope | The oscilloscope has its own colour, independent of the LED colour |
| Chassis | Five chassis themes (Graphite default, JP-8000 Blue, Classic Silver, SH-101 Red, Metaphysical Black). The first four cycle; Metaphysical Black is a fixed explicit theme. |

Three independent user settings, each persisted with the plugin state:

| Setting | Controls | Parameter name used in Simple116 |
|---|---|---|
| Chassis theme | chassis, panels, knobs, buttons, text colours | `guiTheme` |
| LED colour | all LED elements: readouts, LCDs, button LED strips, step bars, knob pointer (see 3.9) | `ledTheme` |
| Scope colour | oscilloscope trace, grid, glow, frame labels | `crtTheme` |

---

## 2. Design tokens

All sizes are **design pixels**. The UI is laid out on a fixed canvas and scaled to the window with `component.setTransform(AffineTransform::scale(windowWidth / canvasWidth))` and a fixed aspect ratio. Simple116 uses 1920 x 1500; other plugins pick their own canvas (section 6, phase 2) but keep every token below at the same design-pixel size so controls look identical across plugins.

### 2.1 Typography

| Role | Font | Height | Kerning (fraction of height) | Used for |
|---|---|---|---|---|
| Caption | Barlow Condensed Bold | 13 (aux knobs 16) | 0.085 | text under knobs |
| Field label | Barlow Condensed Bold | 12 | 0.10 | label above a dropdown |
| Row label / sub-heading | Barlow Condensed Bold | 12-13 | 0.11 / 0.15 | group headings |
| Section title | Barlow Condensed Bold | 16 | 0.15 | panel titles |
| Button text | Barlow Condensed Bold | 14 | 0.10 | button labels |
| Logo | Barlow Condensed Bold | 40 | 0.10 | plugin name |
| Readout | Share Tech Mono | 12 (aux 16) | 0 | LED value plates |
| Dropdown / popup | Share Tech Mono | 13 (popup 20) | 0 | combo text |
| LCD text | Share Tech Mono | 11-13 | 0 | LCD screens, scope labels |

Both fonts are SIL OFL (Google Fonts). Embed as BinaryData (`BarlowCondensed-Bold.ttf`, `ShareTechMono-Regular.ttf`) and include the OFL license text in the repo. The CMake font block must be re-configured after adding files (it checks at configure time).

### 2.2 Chassis themes (colours)

Values are ARGB hex without the leading `ff` unless noted. The kit's `S116Theme.h` contains all of them.

| Token | Graphite | JP-8000 Blue | Classic Silver | SH-101 Red |
|---|---|---|---|---|
| chassis top / body / bottom | 34373e / 1b1d22 / 101114 | 3a5a92 / 1a2d50 / 0c1830 | b9bec7 / a4aab4 / 8c929c | c62732 / a81c27 / 7e121b |
| cheek A / B / C | 3b3e45 / 1c1e22 / 16181b | 3d5f9a / 1a2c4d / 101b32 | d3d7de / a3a9b3 / 8b919b | 5a1018 / 3d0a10 / 2a070b |
| hole dark / ring | 030304 / 6e737c | 030304 / 6e737c | 25272c / eef0f3 | 030304 / d8a5a9 |
| header top / bottom | 2d3037 / 16181c | 2c4a80 / 14233f | 363b44 / 1b1e24 | 8f1822 / 5e0f16 |
| logo / logo accent | d8dce3 / ff3b4e | f1f5fc / ff7b4a | d8dce3 / ff3b4e | f5e9e9 / ffb3b8 |
| panel top / bottom | 2b2e35 / 1f2227 | 2f4f86 / 223e6c | dfe2e8 / c4c9d1 | c0252f / a51d27 |
| scanline alpha | white 0.025 | white 0.025 | white 0.30 | white 0.025 |
| heading / sub / caption text | aab1bd / 9da4b0 / c3c8d1 | dce7f8 / b4c7e6 / eaf1fb | 272c34 / 4a5260 / 14171a | fff4f4 / ffffff / fff4f4 |
| rule (light) | white 0.08 | white 0.08 | black 0.18 | white 0.20 |
| knob body top / bottom | 464b54 / 14161a | 464b54 / 14161a | 9aa0ab / 4a4f59 | 2b2b2e / 0a0a0b |
| knob ring / tick | 3a3e46 / 7d8591 | 7f97c2 / d6e2f6 | white 0.5 / 2b3038 | 7b141c / ffd6d9 |
| cap A / B / sheen alpha | 4b4f58 / 2e3138 / 0.12 | 58627a / 363d52 / 0.12 | e9ecf1 / c3c8d1 / 0.60 | 2a2a2d / 17171a / 0.12 |
| pointer | ff2d45 (follows LED) | ff2d45 (follows LED) | 14171a | ffffff |
| button top / bottom | 4b4f58 / 2e3138 | 58627a / 363d52 | e9ecf1 / c3c8d1 | 2a2a2d / 17171a |
| button ledge / ring | 14161a / 3a3e46 | 14161a / 7f97c2 | 6b717c / white 0.5 | 0a0a0b / 7b141c |
| button text / shadow | c3c8d1 / black (dy -1) | eaf1fb / black (dy -1) | 14171a / white 0.7 (dy +1) | fff4f4 / black (dy -1) |
| dropdown top / bottom / outline | 0e0f12 / 25282e / 4a4f59 | 080e1a / 1a2640 / 8aa2cc | 0e0f12 / 25282e / 4a4f59 | 0e0f12 / 25282e / 8f1822 |
| scope bezel top / bottom | 4b4f58 / 17181c | 58627a / 0f1522 | 4b4f58 / 17181c | 2a2a2d / 17181c |

Constants in all themes: black ring `060607`, dropdown text `ffd0d4`, dropdown arrow `ff5a66`. These two are fixed red-tinted values in kit v1.0.1; they do not follow the LED colour. Deriving them from `ledText` (text at 0.85 brightness, arrow = `ledText`) is a recommended improvement.

**Metaphysical Black (fixed theme, v1.4):** chassis `14161a / 0a0b0d / 050608`; cheeks `1a1c21 / 101216 / 050608`; header `16181d / 0a0b0d`; panel `111216 / 0a0b0d`; heading / sub / caption `e6e9f0 / b8bfcc / c3c8d1`; logo `e6e9f0` with accent `ff2a85`; knob body `2a2d33 / 0c0d10`, ring / tick `3a3e46 / 8a919e`, cap `343841 / 1b1d22`; button `262a31 / 15171b`, ledge / ring `050608 / 3a3e46`, text `e6e9f0`; pointer fixed `ff2a85` (does not follow the LED); dropdown outline `4a4f59`, arrow `ff2a85`; scope bezel `2a2d33 / 0a0b0d`. Set the LED base to `ff2a85` at startup to complete the identity. `ThemeId::MetaphysicalBlack` is outside `numThemes`; the four legacy themes still cycle.

### 2.3 LED colour derivation

Take the user's LED colour, read its hue `h`, and derive every LED token from it. `k = min(1, saturation / 0.8)` keeps near-white LEDs near-white.

| Token | HSV (hue h, saturation x k, value) | Red result |
|---|---|---|
| ledOn (lit button strip, bars) | (h, 0.81, 1.00) | ff3047 |
| ledText (LCD text) | (h, 0.65, 1.00) | ff5a66 |
| ledReadout (value plates) | (h, 0.58, 1.00) | ff6b76 |
| ledOff (unlit strip, LCD top) | (h, 0.78, 0.23) | 3a0d12 |
| lcdBottom | (h, 0.85, 0.10) | 1a0508 |
| plateTop | (h, 0.70, 0.055) | 0e0506 |
| plateBottom | (h, 0.76, 0.165) | 2a0a0e |

The default LED base colour is `ff3047` (`SkinManager::setLedColour(colour)` re-derives everything and notifies listeners; `refreshLed()` re-applies the stored base colour after a chassis change). Pointer colour = `ledOn` in Graphite and JP-8000 only. Classic Silver keeps a dark pointer and SH-101 keeps a white pointer (as in the original design).

### 2.4 Contrast rule

Text must reach WCAG contrast 4.5:1 against what it sits on in every chassis theme. Light button faces (average of top and bottom has perceived brightness above 0.55) use dark text `14171a` with a white 0.7 shadow below; dark faces use the theme text with a black shadow above. Derive text colour from the actual face colour; never hard-code white text on a themed surface.

---

## 3. Component specifications

### 3.1 Knob cell

Anatomy, top to bottom: **halo + body** (rotary), **readout plate**, **caption**.

| Size name | Body diameter | Typical use |
|---|---|---|
| Small | 42 | secondary controls |
| Normal | 52 | default |
| Aux | 60 | routing and depth rows (readout 68 x 28, 16px font and caption) |
| Big | 76 | primary controls (cutoff, level, mix) |

* **Rotary square** `T = body + 2 * halo`. Halo is 11 px (tick marks live there). Rows that contain a waveform-glyph knob use halo 34 for every knob in the row so bodies align.
* **Sweep:** -135 deg to +135 deg (0 = 12 o'clock, clockwise). 11 ticks every 27 deg; a 5-stop waveform knob has 5 ticks every 67.5 deg.
* **Ticks:** from radius `body/2 + 4.5` to `body/2 + 11`, 1.4 px, tick colour at 0.9 alpha.
* **Body:** disc with a vertical gradient (`knobBodyTop` to `knobBodyBottom`). Outside it: a 2 px black ring (`060607`) and a 1 px coloured ring (`knobRing`). Ground shadow: black 0.75, blur 9, offset (0, 5).
* **Cap:** inset `round(body * 0.095)` from the body edge, drawn 2 px higher than the body centre. Fill = 72 thin wedges whose colour swings between `capA` and `capB` as a triangle wave of period 180 deg (a faux conic gradient starting at 20 deg); plus a white sheen gradient from the top-left at `capHighlight` alpha; 1 px outline `0a0b0d`; 1 px white 0.2 highlight arc on the top; drop shadow black 0.7, blur 3, offset (0, 2). Matte: no hard specular.
* **Pointer:** 3 px round line with a 6 px glow stroke (the glow at 0.45 x `pointerGlow` alpha). It starts `inset + 1.5` px from the body edge and is `body * 0.28` long, centred on the cap centre.
* **Waveform glyphs** (5-stop knob only): at radius `body/2 + 24` around the knob at -135, -67.5, 0, +67.5, +135 deg: sine, triangle, saw, square, pulse. Glyph paths in an 18 x 12 box, 1.5 px stroke in `caption` colour:
  `M1 6C3.5 .5 6 .5 9 6C12 11.5 14.5 11.5 17 6` | `M1 6L5 1L13 11L17 6` | `M1 10L9 2V10L17 2` | `M1 10V2H9V10H17` | `M1 10H6V2H9V10H17`
* **Readout plate:** 54 x 20 px (Aux 68 x 28), radius 3, vertical gradient `plateTop` to `plateBottom`, 5 px black inner shadow at the top, 1 px white 0.12 lip at the bottom, mono text `ledReadout` with a faint 4-direction glow (0.18 alpha, 1 px). Gap from body bottom to plate top: 13 px (18 px in halo-34 rows).
* **Caption:** 5 px below the plate, 16 px tall, centred, black 1 px shadow above the text.
* **Heights:** slider component height = `T/2 + body/2 + gap + readoutH`; cell height = that + 5 + 16. Results: Small 107, Normal 117, Big 141, Aux 133, Normal in a glyph row 145.

### 3.2 Ledge button

* **Face** is 34 px high, radius 6. Component bounds = face expanded by 3 left, 3 right, 3 top, 6 bottom (JUCE cannot paint outside bounds): `buttonBoundsFromFace(face)`.
* **Layers (back to front):** ledge outline (`btnRing`, 3 px), ledge black ring (`060607`, 2 px), ledge fill (`btnLedge`, 3 px deep; 1 px when pressed), face black ring (2 px), **flat face fill `btnTop`** (kit v1.0.1 does not use `btnBottom` or a gloss gradient; `btnBottom` only feeds the text-contrast test), bottom inner shade (black 0.35 over the last 8 px), top edge highlight (white 0.2, 1 px). Corner radius is `min(6, width/4, height/4)`; the insets also shrink for very small buttons (`min(3, w/8)`, `min(6, h/6)`), but text placement assumes the standard insets, so keep buttons at least 48 px wide.
* **States:** hover = white 0.05 overlay. Pressed or toggled-on = face moves down 2 px, ledge shrinks to 1 px, overlay black 0.20 plus a 6 px dark inner shadow at the top.
* **LED strip:** 3 px high, inset 10 px from the sides, 5 px from the face top. Off: `ledOff` with a dark top line. On: `ledOn` with two soft glow layers (alpha 0.12 expanded (5, 4) and 0.22 expanded (2.5, 2)). **Only toggled-on buttons light the strip**; momentary buttons never do.
* **Text:** below the strip (trim 9 px from the face top), centred; see 2.4 for colour. Toggle buttons are `juce::TextButton` with `setClickingTogglesState(true)`; convert any `ToggleButton` to `TextButton` first (ButtonAttachment works with both).
* **Compact opt-in (v1.4):** set `button.getProperties().set("s116Compact", true)` for a **26 px face / 34 px component** (3 top + 26 + 5 bottom), radius 4, 2 px ledge, LED strip inset 8 px and 4 px from the face top, 12 px text. Keep the component at least **48 px wide** (42 px face). The property is off by default, so existing controls are unchanged.

### 3.3 Dropdown

**Kit status (important):** in kit v1.0.1 the combo box is drawn by `juce::LookAndFeel_V4::drawComboBox` using the theme colours (`ComboBox::backgroundColourId = selTop`, outline = `selOutline`, arrow colour = `selArrow`), which is what ships in Simple116 (white chevron). The design's LCD plate described below exists in the code but is switched off. Kit v1.1 makes it opt-in with the compile definition `S116_LCD_COMBO=1`; the look has not been verified in a finished plugin, so check snapshots before adopting it.

The design's LCD dropdown: 32 px tall (including its outline). 1 px `selOutline` outline, radius 5, vertical gradient `selTop` to `selBottom`, 5 px black inner shadow at the top, a down-arrow triangle (8 x 6) 11 px from the right edge in the arrow colour, text in mono 13 at 10 px from the left. A field label (12 px) goes above with a 6 px gap: label + gap + box = 52 px. Popup menus: mono 20 px, minimum item height 30 (they inherit the editor transform, so smaller text becomes unreadable).

### 3.4 Labels

Labels select a style through `label.getProperties().set("style", ...)`: `caption`, `field`, `sub`, `accent`. `accent` uses `logoAccent` at 13 px (or `captionSize`). A slider's own text box is drawn as the LED readout plate (property `readout`, set by `createSliderTextBox`).

### 3.5 Section panel

Radius 6, vertical gradient `panelTop` to `panelBottom`, 1 px scanline every 3 px (`scanline`), 1 px white 0.14 top highlight, 4 px black 0.5 bottom shade. Padding 46 top, 22 sides, 20 bottom. Title: Barlow 16 centred between two engraved rules (dark 1 px over light 1 px) with 34 px side margins and a 12 px gap. Two screws (11 px, radial gradient `d6d9de` / `6b7078` at 60% / `2a2d32`) at (12, 13) and (width - 23, 13). Sub-heading: 19 px tall, text left, a rule (light 1 px then dark 1 px) under it.

**Compact mode (v1.4):** `panel.setCompactPadding(true)` switches to padding 24 top / 4 sides / 4 bottom. The title is left-aligned Barlow 16 at (6, 2), one 1 px rule at y = 22, and the optional subtitle (`panel.setSubtitle(...)`, Barlow 12) is right-aligned only when the measured title + subtitle + 24 px fit inside the panel; otherwise it is omitted. No screws or engraved rules in compact mode. `getContentArea()` reflects the chosen padding, so existing layouts are unchanged until they opt in.

### 3.6 Chassis and header

* **Chassis:** vertical gradient `chassisTop` at 0%, `chassisBody` at 5% and 96%, `chassisBottom` at 100%. Rack-ear cheeks 32 px wide at both sides (gradient A at 0%, B at 30%, C at 100%; 2 px white 0.12 line on the left edge, 2 px black 0.7 line on the right edge; holes of 13.6 / 11 px diameter every 150 px starting at y = 75; outer shadow black 0.8, blur 10).
* **Header bar:** 72 px high, radius 6, gradient, 1 px white 0.12 top line. Logo: Barlow 40 with the dash in `logoAccent`, black 1 px shadow above and white 0.2 below.

### 3.7 LCD and displays

* **LCD plate:** radial gradient `lcdTop` (centre) to `lcdBottom` (corner), radius 4, 8 px inner top shadow, 1 px white 0.14 bottom lip. Text mono in `ledText` with the 4-direction glow.
* **Envelope display:** LCD plate, 3 + 3 faint grid lines (`ledText` at 0.15), curve filled with `ledReadout` at 0.28, line `ledReadout` 1.6 px with a 5 px glow stroke at 0.30 (`EnvelopeLcd::setEnvelope(a, d, s, r)` takes proportions of the slider ranges, so a skewed time slider maps through `valueToProportionOfLength`). The curve MUST be driven by the real parameters (proportional segment widths) and update on any parameter change, including preset loads.
* **Step display:** LCD plate with a header strip (mono 11, `ledText`), 16 bars with a vertical gradient (`ledReadout` to `ledReadout` darker by 0.5) and a soft 0.25 halo, the playhead bar brighter by 0.3, step numbers (mono 9, `ledReadout` darker by 0.3). The kit's `StepBarsLcd` is a simple display; Simple116 uses its own richer step component (focus outline, P-lock badges) restyled with the same tokens. Keyboard-driven editing stays as in the host plugin.
* **CRT scope frame:** bezel radius 14 with a diagonal gradient, 12 px bezel width, screen radius 20 with a radial gradient (phosphor x 0.18 brightness at the centre to x 0.05 at the edge), 4 x 4 grid at phosphor 0.18, a vignette, labels in mono 11 (phosphor colour): "<NAME> CRT // <COLOUR>" left, "ZOOM x" right. The trace component must be transparent and draw only the trace: 1.6 px core (phosphor brighter 0.4) over a 6 px glow at 0.30 alpha. The frame is square.

### 3.8 Layout constants

| Constant | Value |
|---|---|
| Panel padding (top / sides / bottom) | 46 / 22 / 20 |
| Sub-heading height | 19 (+6 gap to cells) |
| Gap between rows | equal gaps over the panel (min 8 to 10) |
| Dropdown height | 32 |
| Button face height | 34 (bounds 43) |
| Readout plate | 54 x 20 (Aux 68 x 28) |
| Column width for Normal glyph knobs | 131 minimum; Small 77 to 86 minimum |

### 3.9 Which elements follow which colour setting

| Element | Chassis theme | LED colour | Scope colour |
|---|---|---|---|
| Chassis, panels, header, cheeks | yes | no | no |
| Knob body, cap, ticks, rim | yes | no | no |
| Knob pointer | yes (shape/colour in Silver and SH-101) | Graphite and JP-8000 only | no |
| Buttons (face, ledge, text) | yes | LED strip only | no |
| Readout plates and text, LCD plates and text | no | yes | no |
| Envelope and step displays | no | yes | no |
| Dropdown text and arrow | outline only | not yet (fixed red tint; see 2.2) | no |
| Oscilloscope frame, grid, trace, labels | bezel only | no | yes |

### 3.10 Vertical fader (v1.4, opt-in)

Set `slider.getProperties().set("s116Fader", true)` on a `LinearVertical` slider. The kit draws inside the slider bounds:

* **Slot:** 4 px wide, 4 px inset top and bottom, recessed black, 1 px white 0.10 left edge and black 0.70 right edge.
* **Ticks:** 11 positions, 4 px ticks (6 px at the ends and centre), both sides in `tick` at 0.9 alpha.
* **Cap:** 20 x 36 px, matte `capA`/`capB` gradient, 1 px `0a0b0d` outline, top highlight and a 3 px drop shadow. Pointer line is 3 px with a 6 px glow in `pointer`/`pointerGlow`.
* **LED lane:** 2 px lane on the right, `ledOff` above and `ledOn` from the cap to the slot bottom, with a soft glow.
* **Cell footprint:** the fader slider needs a 96 px minimum height; with a 16 px title, 4 px gap and two stacked 43 px standard buttons the cell is 202 px. With the compact button face (34 px component) it is 184 px. Minimum cell width is `max(54, label width + 4)`; use 68 px for “DUAL NOISE” at 13 px, 64 px for “RESO TUNE”. The fader is opt-in; existing Simple116 linear faders keep their current drawing.

---

## 4. The kit (code)

Five headers plus fonts. Put them in a shared folder and pull them into each plugin (git submodule, or copy; see 4.2).

| File | Role |
|---|---|
| `S116Theme.h` | `Theme` struct, five presets (four cycled + fixed Metaphysical Black), LED derivation (`applyLed`), shared metric constants |
| `SkinManager.h` | singleton `S116::skin()`: `theme()`, `setTheme`, `cycleTheme`, `setLedColour`, `refreshLed`, `getLedBaseColour`, `setTypefaces`, `ui(height, kerning)`, `mono(height)`, `getKnobBase`, `ChangeBroadcaster`; helper `S116::textWidth` |
| `SilverLookAndFeel.h` | `drawRotarySlider`, `drawLinearSlider` (opt-in fader), `getSliderLayout`, `drawButtonBackground/Text` (standard and `s116Compact`), `drawComboBox`, popup font and sizing, `drawLabel` (readout plate and caption/field/sub/accent styles) |
| `S116Widgets.h` | `SkinnedComponent` (cached backdrop pattern; `invalidateSkin()`), `ChassisBackground`, `HeaderBar`, `SectionPanel` (`setCompactPadding`, `setSubtitle`), `StyledText`, `LcdPlate`, `LcdText`, `EnvelopeLcd`, `StepBarsLcd`, `CrtScopeFrame` (`setPhosphor`, `setLabels`, `getScreenBounds`), `KnobCell` (`bindToChoice`, `rebind`, `setHeaderMode`, `setValueTextFormat`), `LabelledCombo`, `LedButton`; helpers `drawScrew`, `glowText` |
| `S116Layout.h` | geometry helpers: `placeKnob`, `placeBareKnob`, `placeButton`, `placeField`, `placeCaptionedCombo`, `distribute`, `DecorSet`, `knobSliderHeight`, `knobCellHeight`, `styleCaption`, `styleField`, `styleSub`, `styleAccent` |
| `Fonts/` | the two TTFs plus OFL license texts |

### 4.1 Property conventions (how the look-and-feel is configured per control)

| Component | Property | Meaning |
|---|---|---|
| Slider | `body` (int) | knob body diameter (42/52/60/76) |
| Slider | `halo` (int) | 11 normal, 34 for glyph rows |
| Slider | `morph` (bool) | draws the 5-stop waveform glyph ring |
| Slider | `readoutW`, `readoutH`, `bigReadout` | readout plate size (Aux uses 68, 28, true) |
| Slider | `readoutRight` (bool) | readout to the right of the knob (compact header use) |
| Slider | `s116Fader` (bool) | opt-in vertical fader art |
| Button | `s116Compact` (bool) | 26 px face / 34 px component / 48 px minimum width |
| Label | `style` ("caption" / "field" / "sub" / "accent") | text style; accent = `logoAccent` |
| Label | `captionSize` (float) | caption font height override (Aux = 16) |
| Label | `readout` (bool), `big` (bool) | LED plate (set automatically on slider text boxes) |

The rotary slider must use `TextBoxBelow` when it has a readout: the layout puts the slider's own text box where the readout plate belongs. Use `placeBareKnob` for a knob with no text box and no caption.

**Two ways to build a knob.** (1) *Re-skinning an existing plugin:* keep your `juce::Slider` (with its text box) and caption `juce::Label`, and position both with `S116::layout::placeKnob(slider, label, x, y, w, body, glyphRow, morph, aux)`; it sets the properties above and the look-and-feel does the rest. This is what Simple116 does. (2) *New code:* `S116::KnobCell(apvts, paramID, caption, options)` owns the slider (no text box), a separate readout label and the caption, and binds the parameter; place it with `getIdealWidth()/getIdealHeight()`. Do not mix the two on one control.

### 4.2 Sharing the kit across plugins

```cmake
# in the shared kit folder: s116-ui/CMakeLists.txt
add_library(s116_ui INTERFACE)
target_include_directories(s116_ui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fonts/BarlowCondensed-Bold.ttf AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fonts/ShareTechMono-Regular.ttf)
    juce_add_binary_data(S116Fonts SOURCES fonts/BarlowCondensed-Bold.ttf fonts/ShareTechMono-Regular.ttf)
    target_link_libraries(s116_ui INTERFACE S116Fonts)
    target_compile_definitions(s116_ui INTERFACE S116_HAS_FONTS=1)
endif()
# in each plugin:  add_subdirectory(path/to/s116-ui)  and  target_link_libraries(MyPlugin PRIVATE s116_ui)
```

The editor sets fonts once, before the first paint:
`S116::skin().setTypefaces (Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize), Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize));` (JUCE removes the hyphen from the file name when it forms the symbol; check the generated `BinaryData.h`). The snippet above is a sketch: use the kit's own `CMakeLists.txt` and README, which are authoritative.

Keep one copy of the kit. Fix bugs in the kit, then update each plugin. Do not fork it per plugin.

---

## 5. Settings and persistence

* Chassis, LED and scope colour are plugin-level settings. Store them as non-automatable parameters (as in Simple116) or as ValueTree properties saved in the plugin state. Defaults: Graphite, red LED, red scope.
* The UI listens for changes and calls `skin().setTheme(...)`, `skin().refreshLed()`, `scopeFrame.setPhosphor(...)`. Labels on the three buttons are always derived from the real state (`"SCOPE: " + name`), never hard-coded.
* Parameter listener callbacks can arrive on any thread. Hop to the message thread with `AsyncUpdater` or `MessageManager::callAsync` before touching components.

---

## 6. Playbook: applying the look to another plugin

### Phase 0: baseline and inventory

1. Create a branch from the current commit; note the commit hash as `ORIG`. Confirm the baseline builds and runs.
2. Generate the **control inventory** (do this before any layout work): a debug function that writes one line per visible descendant of the editor: `member name | class | paramID | text | x,y,w,h`. Set `component.setName("member")` and `component.getProperties().set("paramID", id)` in the attach helpers. Save the editor's member declarations (`PluginEditor.h`) verbatim.
3. List every APVTS parameter and every non-parameter state the UI edits. This is the coverage list: every parameter that is user-facing today must still have a control.
4. List every live component (timer, listener, key handler, data feed, callbacks). Write a **behaviour inventory** (what each does: mouse, keys, timer rate, state it touches).

### Phase 1: install the kit, change nothing else

1. Add the kit, fonts and CMake block. `SilverLookAndFeel laf;` in the editor, `setLookAndFeel(&laf)` (clear it in the destructor). Build and run: every knob, button and dropdown now has the new look in the old layout. Commit.
2. Convert `ToggleButton` to `TextButton` with `setClickingTogglesState(true)`.
3. Slider text boxes: keep `TextBoxBelow`; the LookAndFeel turns them into readout plates. Remove any code that sets text-box colours or fonts after construction.
4. For every rotary slider set the properties of 4.1 through `placeKnob` (phase 3), or manually before then.

### Phase 2: canvas and panels

1. Decide the canvas size. Choose from the plugin's complexity; keep tokens unchanged. Guidance:

| Plugin type | Suggested canvas | Panels |
|---|---|---|
| Single-effect (3 to 8 controls) | 800 x 360 | one panel, one row |
| Multi-section effect (10 to 24 controls) | 1200 x 520 to 700 | 2 to 4 panels |
| Synth or sampler | 1600 to 1920 wide | 6 to 8 panels (Simple116 is 1920 x 1500) |

2. Add `juce::Component content;` and move every `addAndMakeVisible(x)` to `content.addAndMakeVisible(x)` (mechanical search and replace). `content.setSize(canvasW, canvasH)`; in `resized()`: `content.setTransform(AffineTransform::scale((float) getWidth() / canvasW))`; `setResizable(true, true)`, fixed aspect ratio.
3. Add `ChassisBackground`, `HeaderBar` (optional for small plugins) and one `SectionPanel` per logical group, as the first children of `content`. Panel rectangles are fixed; leave a 36 px side margin for the rack-ear cheeks.
4. The old `paint()` must draw nothing (no leftover panels or backgrounds). Remove any old scaling code to avoid double scaling.

### Phase 3: layout (the step that went wrong the first time)

* **Generate layout code from the inventory**, one function per panel, in a single `layoutPanels()` called at the end of `resized()`. Delete the old `setBounds` calls in `resized()`.
* Use the helpers: `placeKnob`, `placeButton`, `placeField`, `placeCaptionedCombo`, `distribute`.
* For each panel: compute the heights of its blocks from the ideal cell heights (3.1), then `distribute(contentHeight, heights, minGap)` for equal gaps. Never stack everything at the top. Check that the sum of heights fits; if not, make the panel taller or split it.
* Columns: divide the panel content width equally; make sure the column is at least the knob square (`T`) and at least the readout width. 131 px columns suit Normal knobs; 86 px suit Small.
* Mixed knob sizes in a row: shift the smaller cells down by half the difference of their squares so body centres line up.
* Buttons are placed by FACE rectangle; tab-style rows get `y + 3` so the face aligns with the row top.
* Group related controls under sub-headings (`DecorSet::sub`). Keep captions short, uppercase, 10 characters or fewer.
* Every ORIG control must be placed. Hidden zero-size labels may stay hidden. Anything that does not fit is reported, not removed.
* Create decorative components (sub-headings) AFTER the real controls so they never block clicks (they have `setInterceptsMouseClicks(false, false)`), and call `toBack()` on backdrops once.

### Phase 4: live components

For every timer-driven or data-driven component (scope, meters, envelope display, step displays, preset/LCD screens): keep the class and its data feed; change only colours, fonts and drawing. Put live drawing in the component's own `paint()` / `paintDynamic()`, never in `paintSkin()` (that is rasterised once). Use the theme and LED colour at draw time, not cached copies.

### Phase 5: colour settings

Wire the three buttons (chassis, LED, scope) to the plugin's settings (section 5). If the plugin has no scope, skip the scope button. Add the LED derivation (`applyLed`) and `refreshLed()` and replace every hard-coded red LED colour in plugin-specific components with theme fields.

### Phase 6: verification

Run section 8 in full. Print the raw output.

---

## 7. Pitfalls (each of these cost real time)

1. **Rewriting the editor.** An agent that rewrote the editor and tried to port logic back lost the sequencer, preset handling, envelope links and the LCD. In-place re-skin only.
2. **Stubs and empty lambdas** pass compilation and fail users. Forbidden.
3. **JUCE cannot paint outside component bounds.** CSS-style outer shadows do not exist. Knob halos (11 / 34 px) and button insets (3 / 3 / 3 / 6) are why those components are larger than their art.
4. **Anything drawn in `paintSkin()` is cached and frozen.** Meters, scopes and playheads must draw elsewhere.
5. **`TextBoxBelow` is required** for the readout layout; `getSliderLayout` positions the plate. Do not call `setTextBoxStyle` with another position afterwards.
6. **Do not overwrite `onValueChange`.** `KnobCell` and other code may rely on it; chain callbacks.
7. **Dropdown heights.** A field label plus box needs 52 px, the box 32. A "sliver" dropdown means the bounds were too small.
8. **Popup menus inherit the editor transform.** Fonts that look right in the editor become tiny in popups. Popup font 20, minimum item height 30.
9. **Default arguments using a nested struct with member initialisers** fail to compile in the same class. Use a delegating constructor.
10. **Fonts are configured at CMake configure time.** Adding the TTFs requires re-running configure.
11. **Parameter IDs in examples are placeholders.** Binding a control to a non-existent parameter ID asserts at runtime and leaves a dead control.
12. **Toggle groups.** If original buttons behave as a radio group, preserve that; but a button that is meant to be an on/off switch (like chorus) must be an independent toggle. Check the original's intent.
13. **Light themes need text colour from the face brightness** (2.4). Never hard-code white text.
14. **Key handling.** A component that edits with the keyboard needs `setWantsKeyboardFocus(true)`, `grabKeyboardFocus()` in `mouseDown`, and `keyPressed` returning `true` only for keys it handles. Removing another component (for example an on-screen keyboard) can change who receives keys.
15. **Standalone has no host transport.** Features that sync to a host need to be tested with host sync off.
16. **Double scaling.** If the old editor scaled itself (fit-scale code), remove it when adding the canvas transform.
17. **Hosts differ on resizable editors.** Keep a fixed aspect ratio and sensible min/max sizes.
18. **The dropdown is not the design's LCD plate** unless `S116_LCD_COMBO=1` is defined (see 3.3). Verify snapshots; do not assume.
19. **`KnobCell::bindToChoice`** in kit v1.0.1 hard-coded a 0..3 range (the Simple116 chorus). v1.1 uses the parameter's real choice count; on v1.0.1 fix it before binding other choice parameters.
20. **Property names are case-sensitive strings** (`"halo"`, `"body"`, `"readoutW"`): a typo silently falls back to the default (halo 11, body 52) and the knob draws at the wrong size.
21. **Light chassis themes need dark text**; the kit chooses text colour from the face brightness in `drawButtonText`. Any text you draw yourself on a themed surface must do the same.

---

## 8. Acceptance checklist (print evidence for each)

1. Build: Release and Debug, zero warnings introduced, no JUCE assertions in a debug run.
2. Control coverage: every visible ORIG control is placed; parameter audit prints only an approved whitelist.
3. Layout audit: no out-of-parent controls; no sibling overlaps except slider vs its own readout/caption and backdrops vs children. Output empty.
4. Every dropdown is 32 px tall; every button face is 34 px; readout plates are 54 x 20 (Aux 68 x 28).
5. Live components: scope, meters and displays change between repaints while audio runs (hash a snapshot of the region over time).
6. Each user setting (chassis, LED, scope): cycle through all choices, print the state, the label text and the colour used; all must agree and persist across a state save and restore.
7. Contrast table for every chassis theme: every text role at least 4.5:1.
8. Snapshot PNGs per chassis theme and per LED colour, written to a folder for human review. Include an enlarged crop of the buttons.
9. Behaviour parity: `git diff ORIG -- <live component files>` contains only colour, font and paint changes; handlers, timers, callbacks and attachments unchanged.
10. Existing presets still load; saved plugin state from the previous version still restores.

---

## 9. Extending the system (design intent; get approval before building)

These controls do not exist in the kit yet. Build them from the same tokens so every plugin stays consistent:

* **Vertical fader:** available as of v1.4; see 3.10 (opt-in `s116Fader`).
* **LED meter:** vertical or horizontal stack of 2 px-gapped segments in `ledOff`, lit segments in `ledOn`, the top 2 segments brighter; peak hold in `ledReadout`.
* **Toggle switch / selector:** use the ledge button; for N-way selectors use a row of buttons as a radio group (the envelope tab row is the model).
* **Preset browser:** `<` dropdown `>` row exactly as the Simple116 header.
* **XY pad:** an LCD plate with a grid in the LED colour at 0.15 and a puck drawn like a small knob cap.

---

## Appendix A: task prompt template for an agent

> You are applying the S-116 design system to the plugin in `<path>`. Read `S116_DESIGN_SYSTEM.md` completely first. Follow section 6 phase by phase. Hard rules: re-skin in place, never rewrite or stub, keep every handler, timer, listener, attachment and callback; no claim without raw printed output; commit after each step; work on a new branch from the current commit.
> Start with phase 0 and report: the control inventory (raw), the parameter list, the live component list with their behaviours. Then stop and wait for confirmation before phase 2. After the layout phase, run the section 8 checklist and report the raw outputs and PNG file list.
> Plugin specifics: `<plugin name, type, canvas size preference, panels/groups you want, any controls that must stay as they are>`.
> If anything in the design system cannot be applied, report the exact blocker (file, line, what is needed). Do not silently change or remove behaviour.

## Appendix B: layout recipe example (reverb plugin, canvas 1200 x 520)

* Header (36, 14, 1128 x 72): logo, preset `<` combo `>`, a master-output Small knob with the readout to its right, CHASSIS and LED COLOR buttons.
* Panel "REVERB" (36, 100, 560 x 400): row 1 Big knobs SIZE and DECAY; row 2 Normal knobs PRE-DELAY, DAMP, DIFFUSION, WIDTH; buttons FREEZE and GATE (LED strips) on the right.
* Panel "MIX" (612, 100, 552 x 400): Big MIX knob, Small LOW CUT and HIGH CUT, a dropdown MODE (label above), an LCD plate showing the decay curve driven by the real parameters.
* Heights from the formulas in 3.1; equal gaps with `distribute`; every control from the original plugin placed.

---

## Appendix C: Synth layout kit (from Simple116, 1920 x 1500)

Use this to lay out every synth in the family the same way. The worked code is `examples/S116EditorLayout.example.inc`.

### C.1 Canvas and panel grid

Margins: 36 left and right (rack-ear cheeks), header at y = 14, 14 px between rows, 12 px between panels in a row, 20 px bottom margin. Row widths always sum to 1848 (1920 - 2 x 36).

| Block | x | y | w | h |
|---|---|---|---|---|
| Header bar | 36 | 14 | 1848 | 72 |
| Oscillators | 36 | 100 | 830 | 583 |
| Filter | 878 | 100 | 560 | 583 |
| Scope | 1450 | 100 | 434 | 583 |
| Envelope | 36 | 697 | 700 | 561 |
| LFO / mod matrix | 748 | 697 | 480 | 561 |
| Sequencer / arp | 1240 | 697 | 644 | 561 |
| Master effects | 36 | 1272 | 1848 | 208 |

Row splits that tile 1848 exactly: 1 panel = 1848; 2 equal = 918 + 12 + 918; 3 equal = 608 + 12 + 608 + 12 + 608; 4 equal = 453 x 4 with 12 gaps; Simple116 uses 830 + 12 + 560 + 12 + 434 and 700 + 12 + 480 + 12 + 644.

### C.2 Planning panel sizes

Panel content area = (x + 22, y + 46, w - 44, h - 66). Block heights (design px):

| Block | Height |
|---|---|
| Sub-heading + gap to its controls | 25 (19 + 6) |
| Knob cell, Small (body 42) | 107 |
| Knob cell, Normal (52) | 117 |
| Knob cell, Aux (60, big readout) | 133 |
| Knob cell, Big (76) | 141 |
| Knob cell, Normal in a glyph row (halo 34) | 145 |
| Dropdown with a field label above | 52 |
| Dropdown with caption below | 52 |
| Button row (face 34) | 43 (face at y + 3) |
| Button pitch in a grid | 44 |
| LCD plates | any (Simple116: envelope 330 x 150, step display 600 x 130, header LCD 260 x 46) |

Formula: `panel height = 66 + sum(block heights) + (blocks + 1) x gap`, gap at least 10 (8 in dense panels). `layout::distribute(contentHeight, heights, minGap)` returns the block y offsets with equal gaps. Simple116's oscillator panel is 10 px under this minimum (blocks 170 + 170 + 155 = 495, gap forced to 8, content 517); the 20 px bottom padding absorbs the overshoot. Do not go beyond about 10 px of overshoot.

Column rule: content width = panel width - 44; the cell column must be at least the knob square (`body + 2 x halo`) and at least the readout width. Minimums that worked: Small 77 to 86, Normal 86 (square 74), glyph row 131 (square 120), Aux 131 (square 82).

### C.3 Panel recipes

* **Header (y 14, h 72, centre y 50):** logo area up to x = 226. Left to right: `<` button (face 38), preset dropdown (300 x 32, y 34), `>` (38), SAVE / INIT / CARD (face 78, gap 8), voice-mode dropdown (190 x 32), master knob (Small 42, `readoutRight`, component 112 x 72 at y 14). Right edge from x = 1860 going left: LCD 260 x 46, LED COLOR button (face 120), CHASSIS button (face 120), gaps 14 then 8. Button faces at y 33.
* **Oscillators (6 columns of 131):** blocks "DCO 1" and "DCO 2" (sub-heading + six glyph-row cells: waveform-glyph knob in column 2, `morph = true`), then "MIXER / PERFORMANCE": four Normal knobs in columns 0 to 3 and a button grid in columns 4 and 5 (`gx = x + 4 x colW + 8`, `gw = 2 x colW - 8`, face width `(gw - 12) / 2`, first row at the knob row's top + 2, pitch 44: SUB, HARD SYNC; PORTA, LEGATO; then a full-width SUB WAVE button).
* **Filter (516 wide):** row A: PARALLEL button (face 110, y + 15), Small FILT BAL knob at x + 140, F2 MODE dropdown (field, 180 wide) at x + 262, y + 6. Row B: four columns of 129; Big cutoff knobs in columns 0 and 2, Normal resonance knobs in columns 1 and 3 placed 12 px lower so knob bodies align. Row C: six Small cells of 86.
* **Scope (390 wide):** square frame 390 x 390 at the content top-left; the oscilloscope component sits at the frame's `getScreenBounds()` (12 px inset) offset by the frame position; the scope colour button (face 170) 18 px under the frame.
* **Envelope (656 wide):** tab row (`<` 40, VCF / VCA / AUX 64 each, `>` 40, gaps 6) at y + 3, then the LCD 330 x 150 at the left, four Small cells of 77 starting at x + 346, vertically centred on the LCD; then a sub-heading and five Aux cells of 131.
* **LFO / mod matrix (436 wide):** block "LFO": two field dropdowns (100 and 120 wide) plus two Small knobs at x + 252 and x + 338. Block "MOD MATRIX": row 1 slot dropdown (110) + ENABLE button (x + 130); row 2 source (200) and destination (220 at x + 216); row 3 via dropdown (200, y + 12) and two Small knobs at x + 216 and x + 318.
* **Sequencer (600 wide):** track row (110 / 74 / 74 buttons then four page buttons of 46), transport row (84 / 76 / 76 / 76 / 84 buttons then a 130 dropdown), step display 600 x 130, step-edit row, four Small timing knobs in columns of 150, ARP row of four dropdowns (width `cw - 16`) with captions below.
* **Master effects (1804 wide):** three sub-sections at 0%, 27% and 73% of the width, controls at `cy = y + 25`: toggle buttons at `cy + 20`, dropdown field at `cy + 6`, Small knob cells at `cy`.

### C.4 Making a different synth

1. List the panels the synth needs from the standard set (oscillators, filter, scope, envelope, LFO/mod, sequencer, effects). Drop the ones it does not have.
2. Pick row splits from C.1 so each row tiles 1848, giving wider panels to the busier sections.
3. Compute each panel's height with C.2; the tallest panel in a row sets the row height; give every panel in a row the same height.
4. Canvas height = 100 + row heights + 14 per row gap + 20. Keep the canvas width at 1920 so panels, cheeks and header are identical across synths; only the height varies.
5. Fill each panel with the recipes in C.3 (blocks can be reused: knob row, button grid, field row, tab row, LCD plus knobs).
6. Pick the knob size by importance: Big for one or two primary controls per panel (cutoff, level), Normal for the main set, Small for secondary groups, Aux for modulation depth rows.

## Appendix D: Known issues in kit v1.0.1 and the v1.1 patch

`patches/v1.1-code.patch` applies cleanly to the v1.0.1 headers (`git apply patches/v1.1-code.patch` from the kit root; verified against the uploaded files). It changes:

* `SilverLookAndFeel.h`: header comment now documents the real property names; the dead `drawComboBox` code after an early `return` becomes an opt-in branch (`S116_LCD_COMBO=1`); default behaviour is unchanged. Removed an unused include.
* `S116Widgets.h`: removed a stray `;`; `KnobCell::bindToChoice` uses the parameter's real choice count instead of 0..3.
* `S116Theme.h`, `SkinManager.h`: stale comments corrected ("NEW FILE", "REPLACES your existing file", a backward-compatibility layer that no longer exists).

Known and left as is (decide per plugin):

* Dropdown text (`ffd0d4`) and arrow (`ff5a66`) do not follow the LED colour.
* `drawButtonText` uses fixed insets while `drawButtonBackground` shrinks them for very small buttons, so tiny buttons can have slightly off-centre text.
* `StepBarsLcd` is a simple display; Simple116's real step editor is plugin code.
* Knob art is cached at 2x; on displays scaled above 2x it is slightly soft.
* `CrtScopeFrame` assumes a square screen area; the scope component and its `setTraceOnly`-style behaviour are plugin code.

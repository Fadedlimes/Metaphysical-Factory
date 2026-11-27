# S-116 UI Kit v1.4

Reusable JUCE UI kit extracted from Simple116. Skeuomorphic hardware-rack look: dark matte chassis, machined knobs, ledge buttons with LED strips, dropdowns, LED readouts, LCD displays and vertical faders. Five chassis themes (Graphite, JP-8000 Blue, Classic Silver, SH-101 Red and the fixed Metaphysical Black), a user-selectable LED colour and an independent scope colour.

Read `docs/S116_DESIGN_SYSTEM.md` first (design tokens, component specs, and the playbook for applying the kit to a plugin). `examples/S116EditorLayout.example.inc` is a complete worked layout of a full synth.

## Files

- `include/S116Theme.h`: `S116::Theme` struct, `ThemeId` enum, five presets, `Theme::applyLed()`, shared metric constants (`kDesignW`, `kDesignH`, `kTickMargin`, `kGlyphMargin`, `kButtonFaceH`, `kBtnIns*`, `kSelectH`, `kReadoutH`, `buttonBoundsFromFace`).
- `include/SkinManager.h`: `S116::SkinManager` singleton (`get()`, `skin()`): chassis theme (`setTheme`, `cycleTheme`, `theme`, `themeId`), LED colour (`setLedColour`, `refreshLed`, `getLedBaseColour`), typefaces (`setTypefaces`, `ui`, `mono`), cached knob art (`getKnobBase`), `ChangeBroadcaster`. Also `S116::textWidth`.
- `include/SilverLookAndFeel.h`: `SilverLookAndFeel`: `applyTheme`, `drawRotarySlider`, `drawLinearSlider` (opt-in kit fader), `getSliderLayout`, `drawButtonBackground`, `drawButtonText` (standard and compact faces), `drawComboBox`, `getComboBoxFont`, `positionComboBoxText`, `createSliderTextBox`, `getPopupMenuFont`, `getIdealPopupMenuItemSize`, `drawLabel` (caption / field / sub / accent).
- `include/S116Widgets.h`: `S116::SkinnedComponent`, `ChassisBackground`, `HeaderBar`, `SectionPanel` (default and compact padding, optional subtitle), `StyledText`, `LcdPlate`, `LcdText`, `EnvelopeLcd`, `StepBarsLcd`, `CrtScopeFrame`, `KnobCell`, `LabelledCombo`, `LedButton`, helpers `drawScrew`, `glowText`.
- `include/S116Layout.h`: `S116::layout` helpers: `placeKnob`, `placeBareKnob`, `placeButton`, `placeField`, `placeCaptionedCombo`, `distribute`, `DecorSet`, `knobSliderHeight`, `knobCellHeight`, `styleCaption`, `styleField`, `styleSub`, `styleAccent`.
- `fonts/`: `BarlowCondensed-Bold.ttf`, `ShareTechMono-Regular.ttf`, OFL license texts.
- `docs/S116_DESIGN_SYSTEM.md`: full design system and playbook.
- `examples/S116EditorLayout.example.inc`: the Simple116 layout (one function per panel) as a reference.

## Two ways to build a control (pick per plugin)

1. **Existing sliders (recommended when re-skinning a plugin):** keep your own `juce::Slider` with `TextBoxBelow` and a caption `juce::Label`, and position them with `S116::layout::placeKnob(...)`. The look-and-feel turns the slider text box into the LED readout plate.
2. **New code:** `S116::KnobCell` creates the slider, readout and caption itself and binds an APVTS parameter (`KnobCell`, `LabelledCombo`, `LedButton`).

## Property conventions

| Component | Property | Meaning |
|---|---|---|
| Slider | `body` (int) | knob body diameter (42/52/60/76) |
| Slider | `halo` (int) | 11 normal, 34 for glyph rows |
| Slider | `morph` (bool) | 5-stop waveform glyph ring |
| Slider | `readoutW`, `readoutH`, `bigReadout` | readout plate size (Aux 68x28) |
| Slider | `readoutRight` (bool) | readout to right of knob |
| Slider | `s116Fader` (bool) | opt-in vertical fader art (slot, matte cap, ticks, LED lane) |
| Button | `s116Compact` (bool) | opt-in compact ledge button: 26 px face / 34 px component / 48 px minimum width |
| Label | `style` ("caption"/"field"/"sub"/"accent") | text style; accent uses `logoAccent` |
| Label | `captionSize` (float) | caption font height override (Aux 16) |
| Label | `readout` (bool), `big` (bool) | LED plate (auto on slider text boxes) |

Rotary sliders use `TextBoxBelow` when they have a readout. `placeBareKnob` is the no-text-box path.

`MetaphysicalBlack` is deliberately outside `ThemeId::numThemes`: the four legacy themes keep their existing cycle, while the new fixed theme is selected explicitly with `skin().setTheme(ThemeId::MetaphysicalBlack)`. Set the LED base to `0xffff2a85` when you want its full pink identity.

## Options

- `S116_LCD_COMBO=1` (compile definition): draw dropdowns as the design's LCD plate with a red arrow. Default 0 draws the `juce::LookAndFeel_V4` combo box using the theme colours (the look shipped in Simple116).
- `S116_HAS_FONTS=1` is defined by the kit's CMake when the font files exist.
- `-DS116_JUCE_DIR=/path/to/juce` on the test/preview configure uses a local JUCE checkout instead of downloading it.

## Preview

```sh
cmake -S test -B build-test -DS116_JUCE_DIR=/path/to/juce
cmake --build build-test -j
./build-test/s116_ui_test --preview
```

`s116_ui_test --preview` opens the kit demo with **1280x820 content** (Metaphysical Black, pink LED base, compact panel, opt-in fader, compact REC/speed buttons, and a four-button ON/OFF row for REC and 1x). Without arguments it paints once, runs the bounds audit and exits with a non-zero code on failure.

`s116_ui_test --bank` builds one real Bank panel and runs the debug bounds audit at the four Bank sizes produced by the 1280x730/1280x820 windows (**426x296, 426x316, 426x340, 426x362**). It then opens the 426x340 Bank panel in a window whose height can be resized from 296 to 362. The audit checks every child against its parent, every sibling pair for overlap, every `Label` text against its cell width, and that each 64 px trim knob centre equals its 68 px fader-cell centre. It also checks fader-tick contrast and LED on/off indicator contrast in all five themes (minimum 3:1).

## Adding to a new plugin

CMake:
```cmake
add_subdirectory(path/to/s116-ui ${CMAKE_BINARY_DIR}/s116_ui)
target_link_libraries(MyPlugin PRIVATE s116_ui)
```
Re-run CMake configure after adding or moving the font files (they are detected at configure time).

Editor:
```cpp
#include "S116Theme.h"
#include "SkinManager.h"
#include "SilverLookAndFeel.h"
#include "S116Widgets.h"
#include "S116Layout.h"

SilverLookAndFeel laf;
// in constructor:
setLookAndFeel(&laf);
S116::skin().setTypefaces(
    juce::Typeface::createSystemTypefaceFor(BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize),
    juce::Typeface::createSystemTypefaceFor(BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize));
// destructor:
setLookAndFeel(nullptr);
```
(The BinaryData symbol names come from the file names with the hyphen removed.)

Wire three user settings to your plugin state: chassis theme (`skin().setTheme`), LED colour (`skin().setLedColour`), and your own scope colour if you have a scope. After any change call `laf.applyTheme(); editor.sendLookAndFeelChange();`.

## Changelog

- 1.4: Metaphysical Black theme (fixed, outside the legacy theme cycle); opt-in vertical kit fader; opt-in compact ledge button; compact `SectionPanel` padding with optional subtitle; `accent` label style; `placeBareKnob`; preview mode in the kit test.
- 1.3: `SkinManager::setTextScale`; legibility appendix.
- 1.2: `HeaderBar::setLogo` / `getLogoRight`.
- 1.1: documentation matched to the real code; opt-in LCD dropdown (`S116_LCD_COMBO`); `KnobCell::bindToChoice` uses the parameter's real choice count; removed stray `;` and stale header comments; added `examples/`.
- 1.0.1: cleanup (Simple116-specific alias and ORIG references removed).
- 1.0: initial extraction.

## Version

1.4

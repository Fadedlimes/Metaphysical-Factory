#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class MetaphysicalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Pure Industrial Matte Black & Neon Pink Palette
    static constexpr uint32_t cChassisBg        = 0xff0a0b0d; // True deep anodized matte black
    static constexpr uint32_t cWellBg           = 0xff111216; // Recessed hardware chassis well
    static constexpr uint32_t cPanelBorder      = 0xff22242a; // Graphite steel border
    static constexpr uint32_t cSilkscreenWhite  = 0xffe6e9f0; // Crisp high-contrast off-white
    static constexpr uint32_t cSilkscreenMuted  = 0xff6e7482; // Secondary industrial silkscreen
    static constexpr uint32_t cNeonPink         = 0xffff2a85; // Hot neon pink accent
    static constexpr uint32_t cNeonPinkDim      = 0xff660a32; // Deep pink backlit glow

    MetaphysicalLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(cChassisBg));
        setColour(juce::Label::textColourId, juce::Colour(cSilkscreenWhite));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff16171b));
        setColour(juce::TextButton::textColourOffId, juce::Colour(cSilkscreenMuted));
    }

    // --- 1. Slender 20px x 36px Concave Fader Cap (Erica Synths Finger-Scoop) ---
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          const juce::Slider::SliderStyle /*style*/, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                   static_cast<float>(width), static_cast<float>(height));

        const float trackWidth = 3.0f;
        const float trackX = bounds.getCentreX() - (trackWidth * 0.5f);

        // A. Full-Length Chassis Slot (Never ends before the cap)
        const float slotTop = bounds.getY() + 4.0f;
        const float slotBottom = bounds.getBottom() - 4.0f;
        const auto slotRect = juce::Rectangle<float>(trackX, slotTop, trackWidth, slotBottom - slotTop);

        g.setColour(juce::Colour(0xff050507));
        g.fillRoundedRectangle(slotRect, 1.5f);

        g.setColour(juce::Colour(0x24ffffff));
        g.drawRoundedRectangle(slotRect, 1.5f, 0.8f);

        // B. Slender Proportions: 20px wide x 36px tall (1:1.8 ratio, not chubby)
        const float capWidth  = 20.0f;
        const float capHeight = 36.0f;
        const float halfCap   = capHeight * 0.5f;

        // C. Clamp Cap Center strictly within slot bounds (Never hangs off into empty space)
        const float clampedCenterY = juce::jlimit(slotTop + (halfCap * 0.85f),
                                                 slotBottom - (halfCap * 0.85f),
                                                 sliderPos);

        const float capX   = bounds.getCentreX() - (capWidth * 0.5f);
        const float capY   = clampedCenterY - halfCap;
        const auto capRect = juce::Rectangle<float>(capX, capY, capWidth, capHeight);

        // 1. Soft Physical Drop Shadow onto Chassis
        g.setColour(juce::Colour(0xd0000000));
        g.fillRoundedRectangle(capRect.translated(0.0f, 3.5f).expanded(1.0f, 0.0f), 2.5f);

        // 2. Base Cap Solid Body
        g.setColour(juce::Colour(0xff14151a));
        g.fillRoundedRectangle(capRect, 2.5f);

        // 3. Top Raised Crown Lip (5px, catches overhead specular highlight)
        const float lipHeight = 5.0f;
        juce::ColourGradient topLipGrad(juce::Colour(0xff3c404b), capX, capY,
                                        juce::Colour(0xff22252c), capX, capY + lipHeight, false);
        g.setGradientFill(topLipGrad);
        g.fillRect(capX + 1.0f, capY + 1.0f, capWidth - 2.0f, lipHeight);

        // Specular highlight line along top edge
        g.setColour(juce::Colour(0xff606675));
        g.drawHorizontalLine(static_cast<int>(capY + 1.0f), capX + 2.0f, capRect.getRight() - 2.0f);

        // 4. Bottom Raised Crown Lip (5px, darker underside)
        const float botLipY = capRect.getBottom() - lipHeight - 1.0f;
        juce::ColourGradient botLipGrad(juce::Colour(0xff0e0f12), capX, botLipY,
                                        juce::Colour(0xff1a1b20), capX, capRect.getBottom() - 1.0f, false);
        g.setGradientFill(botLipGrad);
        g.fillRect(capX + 1.0f, botLipY, capWidth - 2.0f, lipHeight);

        // 5. Deep 3D Concave Finger Scoop (24px hollowed cylindrical bowl)
        const float scoopY = capY + lipHeight + 1.0f;
        const float scoopH = capHeight - (lipHeight * 2.0f) - 2.0f;
        const float midScoopY = scoopY + (scoopH * 0.5f);

        // OPTICAL CONCAVE ILLUSION:
        // Upper slope in deep shadow (#040506), lower slope catches upward light (#343944)!
        juce::ColourGradient concaveGrad(juce::Colour(0xff040506), capX, scoopY,
                                         juce::Colour(0xff343944), capX, scoopY + scoopH, false);
        concaveGrad.addColour(0.48, juce::Colour(0xff020203)); // Deepest trough in shadow
        g.setGradientFill(concaveGrad);
        g.fillRect(capX + 1.0f, scoopY, capWidth - 2.0f, scoopH);

        // Subtle horizontal ridges inside the scoop for finger traction
        g.setColour(juce::Colour(0x12ffffff));
        g.drawHorizontalLine(static_cast<int>(scoopY + scoopH * 0.22f), capX + 2.0f, capRect.getRight() - 2.0f);
        g.drawHorizontalLine(static_cast<int>(scoopY + scoopH * 0.78f), capX + 2.0f, capRect.getRight() - 2.0f);

        // 6. Recessed Hot Pink Indicator Stripe (Sits directly in the deepest trough)
        const juce::Colour stripeCol = slider.isMouseOverOrDragging()
            ? juce::Colour(cNeonPink).brighter(0.2f)
            : juce::Colour(cNeonPink);

        // Ambient Neon Glow
        g.setColour(stripeCol.withAlpha(0.35f));
        g.fillRect(capX + 1.5f, midScoopY - 2.0f, capWidth - 3.0f, 4.0f);

        // Sharp Neon Line
        g.setColour(stripeCol);
        g.fillRect(capX + 1.5f, midScoopY - 0.75f, capWidth - 3.0f, 1.5f);

        // Pure White Center Specular Core
        g.setColour(juce::Colours::white);
        g.fillRect(capX + 3.5f, midScoopY - 0.5f, capWidth - 7.0f, 1.0f);

        // 7. Outer Metallic Edge Bevel
        g.setColour(juce::Colour(0xff2a2d36));
        g.drawRoundedRectangle(capRect, 2.5f, 1.0f);
    }

    // --- 2. Cylindrical Industrial Knob ---
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& /*slider*/) override
    {
        const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                   static_cast<float>(width), static_cast<float>(height)).reduced(3.0f);
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float centreX = bounds.getCentreX();
        const float centreY = bounds.getCentreY();

        // Drop shadow cast by knob
        g.setColour(juce::Colour(0xa0000000));
        g.fillEllipse(centreX - radius + 1.0f, centreY - radius + 3.0f, radius * 2.0f, radius * 2.0f);

        // Knurled Base Skirt
        g.setColour(juce::Colour(0xff1c1e23));
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xff30343d));
        g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.0f);

        // Inset Cylindrical Cap
        const float capRadius = radius * 0.78f;
        juce::ColourGradient capGrad(juce::Colour(0xff26282f), centreX, centreY - capRadius,
                                     juce::Colour(0xff101114), centreX, centreY + capRadius, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2.0f, capRadius * 2.0f);

        g.setColour(juce::Colour(0xff3a3e47));
        g.drawEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

        // Active Indicator Value Arc in Hot Neon Pink
        const float currentAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        juce::Path activeArc;
        activeArc.addCentredArc(centreX, centreY, radius - 2.0f, radius - 2.0f,
                                0.0f, rotaryStartAngle, currentAngle, true);
        g.setColour(juce::Colour(cNeonPink));
        g.strokePath(activeArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Precision Needle
        juce::Path needle;
        needle.addRoundedRectangle(-1.2f, -capRadius + 2.0f, 2.4f, capRadius * 0.5f, 1.0f);
        needle.applyTransform(juce::AffineTransform::rotation(currentAngle).translated(centreX, centreY));

        g.setColour(juce::Colours::white);
        g.fillPath(needle);
    }

    // --- 3. Tactile Push Buttons ---
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& /*backgroundColour*/,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        const bool isToggled = button.getToggleState();

        juce::Colour domeCol = isToggled ? juce::Colour(cNeonPinkDim) : juce::Colour(0xff1a1b20);
        if (shouldDrawButtonAsDown)
            domeCol = domeCol.darker(0.3f);
        else if (shouldDrawButtonAsHighlighted)
            domeCol = domeCol.brighter(0.2f);

        // Button shadow
        g.setColour(juce::Colour(0x90000000));
        g.fillRoundedRectangle(bounds.translated(0.0f, 1.5f), 4.0f);

        // Button body
        g.setColour(domeCol);
        g.fillRoundedRectangle(bounds, 4.0f);

        // Illuminated LED Border when engaged
        if (isToggled)
        {
            g.setColour(juce::Colour(cNeonPink).withAlpha(0.4f));
            g.drawRoundedRectangle(bounds.expanded(1.5f), 5.0f, 2.0f);

            g.setColour(juce::Colour(cNeonPink));
            g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
        }
        else
        {
            g.setColour(juce::Colour(0xff2d3038));
            g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/) override
    {
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(button.getToggleState() ? juce::Colours::white : juce::Colour(cSilkscreenMuted));
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
    }
};
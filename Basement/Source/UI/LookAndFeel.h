#pragma once

#include <JuceHeader.h>

//==============================================================================
//  Palette + typographie : noir profond, une seule couleur d'accent (qui change par artiste),
//  police monospace pour le côté "terminal / underground".
//==============================================================================
namespace theme
{
inline const juce::Colour bg     { 0xff09090a };
inline const juce::Colour ink    { 0xfff1f1f2 };
inline const juce::Colour dim    { 0xff8a8a92 };
inline const juce::Colour faint  { 0xff4a4a52 };
}

inline juce::Font monoFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

class BsmLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BsmLookAndFeel() { setAccent (juce::Colour (0xffff5fb0)); }

    juce::Colour accent;

    void setAccent (juce::Colour c)
    {
        accent = c;
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff111113));
        setColour (juce::PopupMenu::textColourId, theme::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c.withAlpha (0.28f));
        setColour (juce::PopupMenu::highlightedTextColourId, theme::ink);
        setColour (juce::ComboBox::textColourId, theme::ink);
        setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    }

    //--------------------------------------------------------------------------
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAng, float endAng, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (3.f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        auto r = juce::Rectangle<float> (d, d).withCentre (b.getCentre());
        const auto c = r.getCentre();
        const float arcR = d * 0.5f - 2.f;
        const float ang = startAng + pos * (endAng - startAng);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.f, startAng, endAng, true);
        g.setColour (juce::Colour (0x24ffffff));
        g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float a0 = bipolar ? startAng + 0.5f * (endAng - startAng) : startAng;
        if (std::abs (ang - a0) > 0.01f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, arcR, arcR, 0.f, std::min (a0, ang), std::max (a0, ang), true);
            g.setColour (accent.withAlpha (0.22f));
            g.strokePath (val, juce::PathStrokeType (6.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (val, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        auto body = r.reduced (d * 0.17f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff222226), body.getX(), body.getY(),
                                                 juce::Colour (0xff0d0d0f), body.getRight(), body.getBottom(), false));
        g.fillEllipse (body);
        g.setColour (juce::Colour (0x26ffffff));
        g.drawEllipse (body, 1.f);

        const float len = body.getWidth() * 0.5f;
        juce::Path p;
        p.addRoundedRectangle (-1.25f, -len + 2.5f, 2.5f, len * 0.5f, 1.f);
        g.setColour (juce::Colours::white.withAlpha (0.94f));
        g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c));

        if (s.isMouseOverOrDragging())
        {
            g.setColour (accent.withAlpha (0.55f));
            g.drawEllipse (body.expanded (1.5f), 1.f);
        }
    }

    //--------------------------------------------------------------------------
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        auto bounds = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();

        if (b.getButtonText().isEmpty())     // bouton "power" (LED)
        {
            auto r = bounds.withSizeKeepingCentre (10.f, 10.f);
            if (on)
            {
                g.setColour (accent.withAlpha (0.3f));
                g.fillEllipse (r.expanded (4.f));
            }
            g.setColour (on ? accent : juce::Colour (0xff35353a));
            g.fillEllipse (r);
            g.setColour (juce::Colour (on ? 0x00000000 : 0x40ffffff));
            g.drawEllipse (r, 1.f);
            if (highlighted)
            {
                g.setColour (juce::Colour (0x66ffffff));
                g.drawEllipse (r.expanded (3.f), 1.f);
            }
        }
        else                                  // interrupteur avec libellé
        {
            auto pill = bounds.removeFromLeft (28.f).withSizeKeepingCentre (26.f, 14.f);
            g.setColour (on ? accent.withAlpha (0.9f) : juce::Colour (0xff2a2a2e));
            g.fillRoundedRectangle (pill, 7.f);
            const float cx = on ? pill.getRight() - 7.f : pill.getX() + 7.f;
            g.setColour (on ? juce::Colour (0xff09090a) : theme::dim);
            g.fillEllipse (cx - 4.5f, pill.getCentreY() - 4.5f, 9.f, 9.f);

            g.setColour (highlighted || on ? theme::ink : theme::dim);
            g.setFont (monoFont (10.f, true));
            g.drawText (b.getButtonText(), bounds.withTrimmedLeft (4.f), juce::Justification::centredLeft, false);
        }
    }

    //--------------------------------------------------------------------------
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
        g.setColour (juce::Colour (0x10ffffff));
        g.fillRoundedRectangle (r, 3.f);
        g.setColour (box.isPopupActive() || box.isMouseOver() ? accent.withAlpha (0.8f) : juce::Colour (0x2effffff));
        g.drawRoundedRectangle (r, 3.f, 1.f);

        juce::Path p;
        const float cx = (float) w - 11.f, cy = (float) h * 0.5f;
        p.startNewSubPath (cx - 3.5f, cy - 1.5f);
        p.lineTo (cx, cy + 2.f);
        p.lineTo (cx + 3.5f, cy - 1.5f);
        g.setColour (accent);
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return monoFont (11.f, true); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (7, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Font getPopupMenuFont() override { return monoFont (12.f); }
};

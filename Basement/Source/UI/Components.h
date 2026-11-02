#pragma once

#include <JuceHeader.h>
#include <functional>
#include "LookAndFeel.h"
#include "../Params.h"
#include "../DSP.h"

namespace ui
{
inline juce::Colour accentOf (const juce::Component& c)
{
    if (auto* l = dynamic_cast<BsmLookAndFeel*> (&c.getLookAndFeel()))
        return l->accent;
    return juce::Colours::white;
}

//==============================================================================
//  Knob rotatif + libellé. Au survol / pendant le drag, le libellé devient la valeur.
//  Double-clic = valeur par défaut. Molette = réglage fin.
//==============================================================================
class KnobComponent : public juce::Component
{
public:
    KnobComponent (juce::AudioProcessorValueTreeState& apvts, int paramId, const juce::String& labelText,
                   bool bipolar = false, int maxDiameter = 56)
        : label (labelText), param (apvts.getParameter (PID::name (paramId))), maxDiam (maxDiameter)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (220);
        slider.getProperties().set ("bipolar", bipolar);
        addAndMakeVisible (slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, PID::name (paramId), slider);
        slider.addMouseListener (this, false);
        slider.onValueChange = [this] { repaint (labelRect); };
    }

    ~KnobComponent() override { slider.removeMouseListener (this); }

    void resized() override
    {
        const int labelH = 16;
        auto b = getLocalBounds();
        const int d = juce::jmax (12, juce::jmin (b.getWidth(), b.getHeight() - labelH, maxDiam));
        const int total = d + labelH;
        const int top = (b.getHeight() - total) / 2;
        slider.setBounds (juce::Rectangle<int> (d, d).withCentre ({ b.getCentreX(), top + d / 2 }));
        labelRect = juce::Rectangle<int> (0, top + d, b.getWidth(), labelH);
    }

    void paint (juce::Graphics& g) override
    {
        const bool showValue = slider.isMouseOverOrDragging();
        g.setFont (monoFont (10.f, showValue));
        g.setColour (showValue ? accentOf (*this) : theme::dim);
        g.drawText (showValue && param != nullptr ? param->getCurrentValueAsText() : label,
                    labelRect, juce::Justification::centred, false);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint (labelRect); }
    void mouseExit  (const juce::MouseEvent&) override { repaint (labelRect); }
    void mouseUp    (const juce::MouseEvent&) override { repaint (labelRect); }

    juce::Slider slider;

private:
    juce::String label;
    juce::RangedAudioParameter* param;
    int maxDiam;
    juce::Rectangle<int> labelRect;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
//  Panneau de module : cadre, titre, LED on/off, grille de knobs, widgets annexes.
//==============================================================================
class Panel : public juce::Component
{
public:
    Panel (juce::AudioProcessorValueTreeState& a, const juce::String& indexText,
           const juce::String& titleText, int powerParam = -1)
        : apvts (a), idx (indexText), title (titleText)
    {
        if (powerParam >= 0)
        {
            power = std::make_unique<juce::ToggleButton>();
            power->setClickingTogglesState (true);
            addAndMakeVisible (*power);
            powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, PID::name (powerParam), *power);
        }
    }

    KnobComponent* addKnob (int pid, const juce::String& label, bool bipolar = false, int maxDiam = 56)
    {
        auto* k = knobs.add (new KnobComponent (apvts, pid, label, bipolar, maxDiam));
        addAndMakeVisible (k);
        return k;
    }

    juce::ComboBox* addCombo (int pid)
    {
        auto* h = combos.add (new ComboHolder());
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (PID::name (pid))))
            h->box.addItemList (cp->choices, 1);
        h->att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, PID::name (pid), h->box);
        addAndMakeVisible (h->box);
        return &h->box;
    }

    juce::ToggleButton* addToggle (int pid, const juce::String& text)
    {
        auto* h = toggles.add (new ToggleHolder());
        h->btn.setButtonText (text);
        h->btn.setClickingTogglesState (true);
        h->att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, PID::name (pid), h->btn);
        addAndMakeVisible (h->btn);
        return &h->btn;
    }

    void setGrid (int c, int r, bool columnMajor = false, int fixedCellHeight = 0)
    {
        cols = c; rows = r; colMajor = columnMajor; fixedCellH = fixedCellHeight;
    }

    void setTopWidget (juce::Component* c, int h)    { topWidget = c; topH = h; addAndMakeVisible (c); }
    void setSideWidget (juce::Component* c, int w)   { sideWidget = c; sideW = w; addAndMakeVisible (c); }
    void setBottomWidget (juce::Component* c)        { bottomWidget = c; addAndMakeVisible (c); }
    void setColumnHeaders (const juce::StringArray& h) { colHeaders = h; }

    /** Met les contrôles en retrait quand le module est coupé. */
    void refreshActive()
    {
        const bool on = power == nullptr || power->getToggleState();
        if (on == active)
            return;
        active = on;
        for (auto* k : knobs)  k->setAlpha (on ? 1.f : 0.38f);
        for (auto* c : combos) c->box.setAlpha (on ? 1.f : 0.45f);
        for (auto* t : toggles) t->btn.setAlpha (on ? 1.f : 0.45f);
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto head = r.removeFromTop (headerH);
        if (power != nullptr)
            power->setBounds (head.removeFromRight (22).withSizeKeepingCentre (20, 20));
        r.removeFromTop (4);

        if (! combos.isEmpty() || ! toggles.isEmpty())
        {
            auto sub = r.removeFromTop (24);
            const int n = combos.size() + toggles.size();
            const int gap = 6;
            const int w = (sub.getWidth() - gap * (n - 1)) / n;
            for (auto* c : combos)  { c->box.setBounds (sub.removeFromLeft (w)); sub.removeFromLeft (gap); }
            for (auto* t : toggles) { t->btn.setBounds (sub.removeFromLeft (w)); sub.removeFromLeft (gap); }
            r.removeFromTop (6);
        }

        if (topWidget != nullptr)
        {
            topWidget->setBounds (r.removeFromTop (topH));
            r.removeFromTop (6);
        }

        if (sideWidget != nullptr)
        {
            sideWidget->setBounds (r.removeFromRight (sideW));
            r.removeFromRight (6);
        }

        colHeaderRects.clear();
        if (colHeaders.size() > 0)
        {
            auto h = r.removeFromTop (12);
            const int cw = h.getWidth() / cols;
            for (int c = 0; c < cols; ++c)
                colHeaderRects.add (juce::Rectangle<int> (h.getX() + c * cw, h.getY(), cw, h.getHeight()));
        }

        auto grid = r;
        if (fixedCellH > 0)
            grid = r.removeFromTop (fixedCellH * rows);

        const int cw = grid.getWidth() / cols, ch = grid.getHeight() / rows;
        for (int i = 0; i < knobs.size(); ++i)
        {
            const int c  = colMajor ? i / rows : i % cols;
            const int rw = colMajor ? i % rows : i / cols;
            knobs[i]->setBounds (grid.getX() + c * cw, grid.getY() + rw * ch, cw, ch);
        }

        if (bottomWidget != nullptr)
        {
            r.removeFromTop (6);
            bottomWidget->setBounds (r);
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0x0affffff));
        g.fillRoundedRectangle (b, 6.f);
        g.setColour (juce::Colour (0x1affffff));
        g.drawRoundedRectangle (b, 6.f, 1.f);

        auto head = getLocalBounds().reduced (10).removeFromTop (headerH);
        g.setFont (monoFont (10.f, true));
        g.setColour (accentOf (*this).withAlpha (active ? 1.f : 0.5f));
        g.drawText (idx, head.removeFromLeft (20), juce::Justification::centredLeft, false);

        g.setFont (monoFont (12.f, true).withExtraKerningFactor (0.16f));
        g.setColour (active ? theme::ink : theme::dim);
        g.drawText (title, head, juce::Justification::centredLeft, false);

        g.setFont (monoFont (9.f));
        g.setColour (theme::faint);
        for (int i = 0; i < colHeaderRects.size() && i < colHeaders.size(); ++i)
            g.drawText (colHeaders[i], colHeaderRects[i], juce::Justification::centred, false);
    }

private:
    struct ComboHolder  { juce::ComboBox box; std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att; };
    struct ToggleHolder { juce::ToggleButton btn; std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att; };

    juce::AudioProcessorValueTreeState& apvts;
    juce::String idx, title;
    std::unique_ptr<juce::ToggleButton> power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    juce::OwnedArray<KnobComponent> knobs;
    juce::OwnedArray<ComboHolder> combos;
    juce::OwnedArray<ToggleHolder> toggles;

    int cols = 1, rows = 1, fixedCellH = 0, topH = 0, sideW = 0;
    bool colMajor = false, active = true;
    juce::Component *topWidget = nullptr, *sideWidget = nullptr, *bottomWidget = nullptr;
    juce::StringArray colHeaders;
    juce::Array<juce::Rectangle<int>> colHeaderRects;
    static constexpr int headerH = 26;
};

//==============================================================================
//  Courbe de réponse de l'EQ (calculée avec les mêmes biquads que le DSP)
//==============================================================================
class EqCurve : public juce::Component
{
public:
    explicit EqCurve (juce::AudioProcessorValueTreeState& a) : apvts (a) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const auto acc = accentOf (*this);
        const bool on = value (PID::eq_on) > 0.5f;

        g.setColour (juce::Colour (0x40000000));
        g.fillRoundedRectangle (r, 4.f);
        g.setColour (juce::Colour (0x16ffffff));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.f, 1.f);

        auto freqToX = [&] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
        auto dbToY   = [&] (double db) { return juce::jmap ((float) juce::jlimit (-18.0, 18.0, db), -18.f, 18.f, r.getBottom() - 3.f, r.getY() + 3.f); };

        g.setColour (juce::Colour (0x12ffffff));
        for (double f : { 100.0, 1000.0, 10000.0 })
            g.drawVerticalLine ((int) freqToX (f), r.getY() + 2.f, r.getBottom() - 2.f);
        g.setColour (juce::Colour (0x22ffffff));
        g.drawHorizontalLine ((int) dbToY (0.0), r.getX() + 2.f, r.getRight() - 2.f);

        static constexpr int ids[4][3] = {
            { PID::eq1_freq, PID::eq1_q, PID::eq1_gain }, { PID::eq2_freq, PID::eq2_q, PID::eq2_gain },
            { PID::eq3_freq, PID::eq3_q, PID::eq3_gain }, { PID::eq4_freq, PID::eq4_q, PID::eq4_gain } };

        bsm::Biquad bq[4];
        for (int i = 0; i < 4; ++i)
            bsm::configureEqBand (bq[i], i, 48000.0, value (ids[i][0]), value (ids[i][1]), value (ids[i][2]));

        juce::Path curve;
        const int w = (int) r.getWidth();
        for (int px = 0; px <= w; px += 2)
        {
            const double f = 20.0 * std::pow (1000.0, (double) px / (double) w);
            double db = 0.0;
            for (auto& b : bq) db += b.magnitudeDb (f, 48000.0);
            const float y = dbToY (db), x = r.getX() + (float) px;
            if (px == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }

        juce::Path fill (curve);
        fill.lineTo (r.getRight(), dbToY (0.0));
        fill.lineTo (r.getX(), dbToY (0.0));
        fill.closeSubPath();

        const float alpha = on ? 1.f : 0.3f;
        g.setColour (acc.withAlpha (0.14f * alpha));
        g.fillPath (fill);
        g.setColour (acc.withAlpha (alpha));
        g.strokePath (curve, juce::PathStrokeType (1.6f));

        for (int i = 0; i < 4; ++i)
        {
            const float x = freqToX (value (ids[i][0])), y = dbToY (value (ids[i][2]));
            g.setColour (juce::Colours::white.withAlpha (0.85f * alpha));
            g.fillEllipse (x - 2.5f, y - 2.5f, 5.f, 5.f);
        }
    }

private:
    float value (int id) const { return apvts.getRawParameterValue (PID::name (id))->load(); }
    juce::AudioProcessorValueTreeState& apvts;
};

//==============================================================================
//  Indicateur de gain reduction (barre verticale qui descend)
//==============================================================================
class GrMeter : public juce::Component
{
public:
    void setValue (float grDb)
    {
        grDb = juce::jlimit (0.f, 24.f, grDb);
        const float v = grDb > shown ? grDb : shown * 0.86f + grDb * 0.14f;
        if (std::abs (v - shown) > 0.02f) { shown = v; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre (8.f, (float) getHeight());
        g.setColour (juce::Colour (0x14ffffff));
        g.fillRoundedRectangle (r, 3.f);
        auto fill = r.withHeight (r.getHeight() * shown / 24.f);
        g.setColour (accentOf (*this));
        g.fillRoundedRectangle (fill, 3.f);

        g.setFont (monoFont (9.f));
        g.setColour (theme::dim);
        g.drawText ("GR", getLocalBounds().removeFromBottom (12), juce::Justification::centred, false);
    }

private:
    float shown = 0.f;
};

//==============================================================================
//  Affichage de la note détectée / corrigée + jauge de justesse (en cents)
//==============================================================================
class PitchReadout : public juce::Component
{
public:
    void update (float detectedMidi, float targetMidi, bool tuneOn)
    {
        if (detectedMidi != det || targetMidi != tgt || tuneOn != on)
        {
            det = detectedMidi; tgt = targetMidi; on = tuneOn;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const auto acc = accentOf (*this);

        g.setColour (juce::Colour (0x40000000));
        g.fillRoundedRectangle (r, 4.f);
        g.setColour (juce::Colour (0x16ffffff));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.f, 1.f);

        const bool voiced = on && tgt >= 0.f && det >= 0.f;

        auto top = getLocalBounds().reduced (8, 6);
        g.setFont (monoFont (9.f));
        g.setColour (theme::faint);
        g.drawText ("TARGET NOTE", top.removeFromTop (12), juce::Justification::centredLeft, false);

        g.setFont (monoFont (38.f, true));
        g.setColour (voiced ? acc : theme::faint);
        g.drawText (voiced ? noteName (tgt) : juce::String ("--"), top.removeFromTop (48), juce::Justification::centred, false);

        g.setFont (monoFont (10.f));
        g.setColour (theme::dim);
        const float cents = voiced ? (det - tgt) * 100.f : 0.f;
        g.drawText (voiced ? "IN " + noteName (std::round (det)) + "  " + juce::String (cents > 0 ? "+" : "") + juce::String (juce::roundToInt (cents)) + " c"
                           : (on ? juce::String ("listening...") : juce::String ("tune off")),
                    top.removeFromTop (14), juce::Justification::centred, false);

        // jauge ±50 cents
        auto gauge = top.removeFromBottom (16).reduced (4, 0).toFloat();
        const float cy = gauge.getCentreY();
        g.setColour (juce::Colour (0x26ffffff));
        g.drawHorizontalLine ((int) cy, gauge.getX(), gauge.getRight());
        for (int c = -50; c <= 50; c += 25)
        {
            const float x = gauge.getCentreX() + (float) c / 50.f * gauge.getWidth() * 0.5f;
            g.drawVerticalLine ((int) x, cy - (c == 0 ? 5.f : 3.f), cy + (c == 0 ? 5.f : 3.f));
        }
        if (voiced)
        {
            const float x = gauge.getCentreX() + juce::jlimit (-50.f, 50.f, cents) / 50.f * gauge.getWidth() * 0.5f;
            g.setColour (acc);
            g.fillEllipse (x - 3.5f, cy - 3.5f, 7.f, 7.f);
        }
    }

private:
    static juce::String noteName (float midi)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int m = juce::roundToInt (midi);
        return juce::String (names[((m % 12) + 12) % 12]) + juce::String (m / 12 - 1);
    }

    float det = -1.f, tgt = -1.f;
    bool on = false;
};

//==============================================================================
//  Flèche (‹ ›) pour changer d'artiste
//==============================================================================
class ChevronButton : public juce::Button
{
public:
    explicit ChevronButton (bool pointsRight) : juce::Button ("chevron"), right (pointsRight) {}

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto r = getLocalBounds().toFloat();
        const float cx = r.getCentreX(), cy = r.getCentreY(), s = down ? 6.f : 7.f;
        juce::Path p;
        if (right) { p.startNewSubPath (cx - s * 0.5f, cy - s); p.lineTo (cx + s * 0.5f, cy); p.lineTo (cx - s * 0.5f, cy + s); }
        else       { p.startNewSubPath (cx + s * 0.5f, cy - s); p.lineTo (cx - s * 0.5f, cy); p.lineTo (cx + s * 0.5f, cy + s); }
        g.setColour (highlighted ? juce::Colours::white : accentOf (*this));
        g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    bool right;
};

} // namespace ui

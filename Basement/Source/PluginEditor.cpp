#include "PluginEditor.h"

//==============================================================================
static float levelToUnit (float lin)
{
    return juce::jlimit (0.f, 1.f, (bsm::linToDb (lin) + 60.f) / 60.f);
}

//==============================================================================
BasementUI::BasementUI (BasementAudioProcessor& p)
    : proc (p), apvts (p.apvts),
      eqCurve (p.apvts),
      inputP (p.apvts, "01", "INPUT"),
      tuneP  (p.apvts, "02", "TUNE",     PID::tune_on),
      eqP    (p.apvts, "03", "EQ",       PID::eq_on),
      compP  (p.apvts, "04", "COMP",     PID::comp_on),
      deessP (p.apvts, "05", "DE-ESS",   PID::deess_on),
      satP   (p.apvts, "06", "SATURATE", PID::sat_on),
      dblP   (p.apvts, "07", "DOUBLE",   PID::dbl_on),
      dlyP   (p.apvts, "08", "DELAY",    PID::dly_on),
      revP   (p.apvts, "09", "REVERB",   PID::rev_on),
      outP   (p.apvts, "10", "OUTPUT")
{
    setLookAndFeel (&lnf);
    grain = makeGrain();
    setOpaque (true);

    // ------------------------------------------------------------------ 01 INPUT
    inputP.addKnob (PID::in_gain,     "GAIN", true);
    inputP.addKnob (PID::hpf_freq,    "HPF");
    inputP.addKnob (PID::gate_thresh, "GATE");
    inputP.setGrid (1, 3);

    // ------------------------------------------------------------------ 02 TUNE
    tuneP.addCombo (PID::tune_key);
    tuneP.addCombo (PID::tune_scale);
    tuneP.addKnob (PID::tune_speed,  "SPEED");
    tuneP.addKnob (PID::tune_amount, "AMOUNT");
    tuneP.addKnob (PID::tune_shift,  "SHIFT", true);
    tuneP.setGrid (3, 1, false, 84);
    tuneP.setBottomWidget (&pitch);

    // ------------------------------------------------------------------ 03 EQ
    const struct { int gain, freq, q; } bands[4] = {
        { PID::eq1_gain, PID::eq1_freq, PID::eq1_q }, { PID::eq2_gain, PID::eq2_freq, PID::eq2_q },
        { PID::eq3_gain, PID::eq3_freq, PID::eq3_q }, { PID::eq4_gain, PID::eq4_freq, PID::eq4_q } };
    for (auto& b : bands)
    {
        eqP.addKnob (b.gain, "GAIN", true, 40);
        eqP.addKnob (b.freq, "FREQ", false, 40);
        eqP.addKnob (b.q,    "Q",    false, 40);
    }
    eqP.setColumnHeaders ({ "LOW", "LOW-MID", "HI-MID", "AIR" });
    eqP.setGrid (4, 3, true);
    eqP.setTopWidget (&eqCurve, 64);

    // ------------------------------------------------------------------ 04 COMP
    compP.addKnob (PID::comp_thresh,  "THRESH");
    compP.addKnob (PID::comp_ratio,   "RATIO");
    compP.addKnob (PID::comp_attack,  "ATTACK");
    compP.addKnob (PID::comp_release, "RELEASE");
    compP.addKnob (PID::comp_makeup,  "MAKEUP");
    compP.addKnob (PID::comp_mix,     "MIX");
    compP.setGrid (2, 3);
    compP.setSideWidget (&grMeter, 16);

    // ------------------------------------------------------------------ 05 DE-ESS
    deessP.addKnob (PID::deess_freq,   "FREQ");
    deessP.addKnob (PID::deess_thresh, "THRESH");
    deessP.addKnob (PID::deess_amount, "AMOUNT");
    deessP.setGrid (3, 1);

    // ------------------------------------------------------------------ 06 SATURATE
    satP.addCombo (PID::sat_type);
    satP.addKnob (PID::sat_drive, "DRIVE");
    satP.addKnob (PID::sat_tone,  "TONE", true);
    satP.addKnob (PID::sat_mix,   "MIX");
    satP.setGrid (3, 1);

    // ------------------------------------------------------------------ 07 DOUBLE
    dblP.addKnob (PID::dbl_rate,  "RATE");
    dblP.addKnob (PID::dbl_depth, "DEPTH");
    dblP.addKnob (PID::dbl_mix,   "MIX");
    dblP.setGrid (3, 1);

    // ------------------------------------------------------------------ 08 DELAY
    dlyP.addCombo (PID::dly_sync);
    dlyP.addToggle (PID::dly_pingpong, "PING-PONG");
    delayTimeKnob = dlyP.addKnob (PID::dly_time, "TIME");
    dlyP.addKnob (PID::dly_fb,   "FEEDBACK");
    dlyP.addKnob (PID::dly_tone, "TONE");
    dlyP.addKnob (PID::dly_mix,  "MIX");
    dlyP.setGrid (4, 1);

    // ------------------------------------------------------------------ 09 REVERB
    revP.addKnob (PID::rev_size,     "SIZE");
    revP.addKnob (PID::rev_damp,     "DAMP");
    revP.addKnob (PID::rev_width,    "WIDTH");
    revP.addKnob (PID::rev_predelay, "PRE-DLY");
    revP.addKnob (PID::rev_lowcut,   "LOW CUT");
    revP.addKnob (PID::rev_mix,      "MIX");
    revP.setGrid (6, 1);

    // ------------------------------------------------------------------ 10 OUTPUT
    outP.addToggle (PID::out_clip, "SAFE CLIP");
    outP.addKnob (PID::out_gain, "GAIN", true);
    outP.addKnob (PID::out_mix,  "DRY/WET");
    outP.setGrid (2, 1);

    for (auto* panel : { &inputP, &tuneP, &eqP, &compP, &deessP, &satP, &dblP, &dlyP, &revP, &outP })
        addAndMakeVisible (panel);

    for (auto* panel : { &inputP, &tuneP, &eqP, &compP, &deessP, &satP, &dblP, &dlyP, &revP, &outP })
        panel->refreshActive();     // modules éteints grisés dès l'ouverture

    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);
    const int count = (int) getPresets().size();
    prevBtn.onClick = [this, count] { selectPreset ((shownPreset + count - 1) % count); };
    nextBtn.onClick = [this, count] { selectPreset ((shownPreset + 1) % count); };

    setSize (kWidth, kHeight);

    const auto idx = proc.getCurrentProgram();
    shownPreset = idx;
    currentAccent = targetAccent = juce::Colour (getPresets()[(size_t) idx].accent);
    lnf.setAccent (currentAccent);

    startTimerHz (30);
}

BasementUI::~BasementUI()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
juce::Image BasementUI::makeGrain()
{
    juce::Image img (juce::Image::ARGB, 128, 128, true);
    juce::Random rng (20260101);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
        {
            const juce::uint8 v = rng.nextBool() ? 255 : 0;
            img.setPixelAt (x, y, juce::Colour::fromRGBA (v, v, v, (juce::uint8) rng.nextInt (13)));
        }
    return img;
}

//==============================================================================
void BasementUI::selectPreset (int index)
{
    proc.applyPreset (index);
    shownPreset = proc.getCurrentProgram();
    targetAccent = juce::Colour (getPresets()[(size_t) shownPreset].accent);
    repaint (headerArea);
}

void BasementUI::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& presets = getPresets();
    for (size_t i = 0; i < presets.size(); ++i)
        menu.addItem ((int) i + 1, presets[i].name, true, (int) i == shownPreset);

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea (localAreaToGlobal (nameArea))
                            .withMinimumWidth (220),
                        [safe = juce::Component::SafePointer<BasementUI> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->selectPreset (result - 1);
                        });
}

void BasementUI::mouseDown (const juce::MouseEvent& e)
{
    if (nameArea.contains (e.getPosition()))
        showPresetMenu();
}

//==============================================================================
void BasementUI::timerCallback()
{
    // preset changé depuis la DAW (liste de programmes) ?
    const int idx = proc.getCurrentProgram();
    if (idx != shownPreset)
    {
        shownPreset = idx;
        targetAccent = juce::Colour (getPresets()[(size_t) idx].accent);
    }

    // transition douce de la couleur d'accent
    if (currentAccent != targetAccent)
    {
        currentAccent = currentAccent.interpolatedWith (targetAccent, 0.2f);
        const int diff = std::abs ((int) currentAccent.getRed()   - (int) targetAccent.getRed())
                       + std::abs ((int) currentAccent.getGreen() - (int) targetAccent.getGreen())
                       + std::abs ((int) currentAccent.getBlue()  - (int) targetAccent.getBlue());
        if (diff <= 4)
            currentAccent = targetAccent;
        lnf.setAccent (currentAccent);
        repaint();
    }

    // vumètres
    inDisp  = std::max (levelToUnit (proc.meterIn.exchange (0.f)),  inDisp  * 0.86f);
    outDisp = std::max (levelToUnit (proc.meterOut.exchange (0.f)), outDisp * 0.86f);
    grMeter.setValue (proc.meterGR.exchange (0.f));
    repaint (headerArea);

    // note détectée
    pitch.update (proc.getDetectedMidi(), proc.getTargetMidi(),
                  apvts.getRawParameterValue (PID::name (PID::tune_on))->load() > 0.5f);

    eqCurve.repaint();

    for (auto* panel : { &inputP, &tuneP, &eqP, &compP, &deessP, &satP, &dblP, &dlyP, &revP, &outP })
        panel->refreshActive();

    // le knob TIME est inutile quand le delay est synchronisé au tempo
    if (apvts.getRawParameterValue (PID::name (PID::dly_on))->load() > 0.5f)
        delayTimeKnob->setAlpha (apvts.getRawParameterValue (PID::name (PID::dly_sync))->load() > 0.5f ? 0.3f : 1.f);
}

//==============================================================================
void BasementUI::resized()
{
    auto area = getLocalBounds().reduced (16);
    headerArea = area.removeFromTop (64);
    area.removeFromTop (8);
    auto row1 = area.removeFromTop (296);
    area.removeFromTop (8);
    auto row2 = area.removeFromTop (154);
    footerArea = area;

    constexpr int gap = 8;

    // --- ligne 1
    inputP.setBounds (row1.removeFromLeft (96));   row1.removeFromLeft (gap);
    tuneP.setBounds  (row1.removeFromLeft (212));  row1.removeFromLeft (gap);
    eqP.setBounds    (row1.removeFromLeft (336));  row1.removeFromLeft (gap);
    compP.setBounds  (row1.removeFromLeft (190));  row1.removeFromLeft (gap);
    deessP.setBounds (row1.removeFromTop (134));   row1.removeFromTop (gap);
    satP.setBounds   (row1);

    // --- ligne 2
    dblP.setBounds (row2.removeFromLeft (212));  row2.removeFromLeft (gap);
    dlyP.setBounds (row2.removeFromLeft (276));  row2.removeFromLeft (gap);
    revP.setBounds (row2.removeFromLeft (428));  row2.removeFromLeft (gap);
    outP.setBounds (row2);

    // --- en-tête
    nameArea = headerArea.withSizeKeepingCentre (380, 64);
    prevBtn.setBounds (nameArea.getX() - 36, headerArea.getY() + 4, 32, 36);
    nextBtn.setBounds (nameArea.getRight() + 4, headerArea.getY() + 4, 32, 36);
}

//==============================================================================
void BasementUI::paint (juce::Graphics& g)
{
    const auto accent = lnf.accent;
    const float w = (float) getWidth(), h = (float) getHeight();

    g.fillAll (theme::bg);

    // halo d'accent derrière le nom de l'artiste
    {
        juce::ColourGradient glow (accent.withAlpha (0.16f), w * 0.5f, 0.f,
                                   juce::Colours::transparentBlack, w * 0.5f, 430.f, true);
        g.setGradientFill (glow);
        g.fillRect (getLocalBounds());
    }

    // grain de pellicule
    g.setTiledImageFill (grain, 0, 0, 1.f);
    g.fillAll();

    // vignettage
    {
        juce::ColourGradient vig (juce::Colours::transparentBlack, w * 0.5f, h * 0.5f,
                                  juce::Colours::black.withAlpha (0.55f), 0.f, 0.f, true);
        g.setGradientFill (vig);
        g.fillRect (getLocalBounds());
    }

    // ------------------------------------------------------------------ en-tête
    auto logo = headerArea.withWidth (260);
    g.setColour (theme::ink);
    g.setFont (monoFont (24.f, true).withExtraKerningFactor (0.28f));
    g.drawText ("BASEMENT", logo.removeFromTop (34), juce::Justification::centredLeft, false);
    g.setColour (theme::dim);
    g.setFont (monoFont (10.f).withExtraKerningFactor (0.12f));
    g.drawText ("VOCAL CHAIN  //  v0.1", logo.removeFromTop (14), juce::Justification::centredLeft, false);

    const auto& preset = getPresets()[(size_t) juce::jlimit (0, (int) getPresets().size() - 1, shownPreset)];

    auto nameRect = nameArea.withHeight (40).translated (0, 2);
    g.setFont (monoFont (30.f, true).withExtraKerningFactor (0.12f));
    g.setColour (accent.withAlpha (0.65f));
    g.drawText (preset.name, nameRect.translated (2, 0), juce::Justification::centred, false);
    g.setColour (juce::Colour (0xff3df0ff).withAlpha (0.22f));
    g.drawText (preset.name, nameRect.translated (-2, 0), juce::Justification::centred, false);
    g.setColour (theme::ink);
    g.drawText (preset.name, nameRect, juce::Justification::centred, false);

    g.setFont (monoFont (11.f));
    g.setColour (theme::dim);
    g.drawText (preset.tagline, nameArea.withTrimmedTop (42).withHeight (16), juce::Justification::centred, false);

    // vumètres
    auto meterArea = headerArea.removeFromRight (220).reduced (0, 10);
    auto drawMeter = [&] (juce::Rectangle<int> row, const juce::String& label, float unit)
    {
        g.setFont (monoFont (10.f, true));
        g.setColour (theme::dim);
        g.drawText (label, row.removeFromLeft (34), juce::Justification::centredLeft, false);
        auto bar = row.toFloat().withSizeKeepingCentre ((float) row.getWidth(), 6.f);
        g.setColour (juce::Colour (0x18ffffff));
        g.fillRoundedRectangle (bar, 3.f);
        g.setColour (accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * unit), 3.f);
        if (unit > 0.985f)
        {
            g.setColour (juce::Colour (0xffff3b2f));
            g.fillRoundedRectangle (bar.removeFromRight (8.f), 3.f);
        }
    };
    drawMeter (meterArea.removeFromTop (20), "IN", inDisp);
    meterArea.removeFromTop (6);
    drawMeter (meterArea.removeFromTop (20), "OUT", outDisp);

    // ------------------------------------------------------------------ pied de page
    g.setFont (monoFont (9.f));
    g.setColour (theme::faint);
    g.drawText ("tweak everything. trust your ears.", footerArea, juce::Justification::centredLeft, false);
    if (proc.getSampleRate() > 0.0)
        g.drawText ("latency " + juce::String (proc.getLatencyInSamples() * 1000.0 / proc.getSampleRate(), 1) + " ms",
                    footerArea, juce::Justification::centredRight, false);
}

//==============================================================================
BasementAudioProcessorEditor::BasementAudioProcessorEditor (BasementAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), ui (p)
{
    addAndMakeVisible (ui);

    setResizable (true, true);
    setResizeLimits (BasementUI::kWidth * 3 / 4, BasementUI::kHeight * 3 / 4,
                     BasementUI::kWidth * 3 / 2, BasementUI::kHeight * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) BasementUI::kWidth / (double) BasementUI::kHeight);
    setSize (BasementUI::kWidth, BasementUI::kHeight);
}

void BasementAudioProcessorEditor::resized()
{
    ui.setBounds (0, 0, BasementUI::kWidth, BasementUI::kHeight);
    ui.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) BasementUI::kWidth));
}

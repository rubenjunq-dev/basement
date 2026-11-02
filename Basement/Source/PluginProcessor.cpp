#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BasementAudioProcessor::BasementAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    for (int i = 0; i < PID::Count; ++i)
    {
        params[(size_t) i] = apvts.getRawParameterValue (PID::name (i));
        jassert (params[(size_t) i] != nullptr);
    }

    applyPreset (1);   // premier artiste chargé par défaut
}

//==============================================================================
bool BasementAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    auto ok = [] (const juce::AudioChannelSet& s)
    {
        return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo();
    };

    return ok (in) && ok (out);
}

//==============================================================================
void BasementAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;

    hpf.prepare (sr);   gate.prepare (sr);   tune.prepare (sr);   eq.prepare (sr);
    comp.prepare (sr);  deess.prepare (sr);  sat.prepare (sr);    dbl.prepare (sr);
    dly.prepare (sr);   rev.prepare (sr);

    latency = tune.getLatencySamples();
    setLatencySamples (latency);
    for (auto& l : dryLine)
        l.prepare (latency + 8);

    inGainSm.reset (sr, 0.02);   inGainSm.setCurrentAndTargetValue (bsm::dbToLin (P (PID::in_gain)));
    outGainSm.reset (sr, 0.02);  outGainSm.setCurrentAndTargetValue (bsm::dbToLin (P (PID::out_gain)));
    mixSm.reset (sr, 0.02);      mixSm.setCurrentAndTargetValue (P (PID::out_mix) * 0.01f);

    auto initToggle = [this] (juce::SmoothedValue<float>& s, int id)
    {
        s.reset (sr, 0.015);
        s.setCurrentAndTargetValue (P (id) > 0.5f ? 1.f : 0.f);
    };
    initToggle (tgEq, PID::eq_on);       initToggle (tgComp, PID::comp_on);
    initToggle (tgDeess, PID::deess_on); initToggle (tgSat, PID::sat_on);
    initToggle (tgDbl, PID::dbl_on);     initToggle (tgDly, PID::dly_on);
    initToggle (tgRev, PID::rev_on);
}

//==============================================================================
//  Applique `fn` au bloc, avec un fondu enchaîné de 15 ms quand le module est (dés)activé.
template <typename F>
void BasementAudioProcessor::runToggle (juce::SmoothedValue<float>& s, float* const* d, int n, F&& fn)
{
    if (s.getTargetValue() < 0.5f && ! s.isSmoothing())
        return;

    float dryA[bsm::kChunk], dryB[bsm::kChunk];
    std::memcpy (dryA, d[0], (size_t) n * sizeof (float));
    std::memcpy (dryB, d[1], (size_t) n * sizeof (float));

    fn();

    for (int i = 0; i < n; ++i)
    {
        const float g = s.getNextValue();
        d[0][i] = dryA[i] + (d[0][i] - dryA[i]) * g;
        d[1][i] = dryB[i] + (d[1][i] - dryB[i]) * g;
    }
}

//==============================================================================
void BasementAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int total  = buffer.getNumSamples();

    if (numIn < 1 || numOut < 1)
    {
        buffer.clear();
        return;
    }

    // Tempo de l'hôte (pour le delay synchronisé)
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 20.0)
                    bpm = *b;

    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : buffer.getReadPointer (0);   // mono → dupliqué

    float cl[bsm::kChunk], cr[bsm::kChunk], dl[bsm::kChunk], dr[bsm::kChunk];
    float* d[2] = { cl, cr };

    float peakIn = 0.f, peakOut = 0.f, maxGR = 0.f;

    for (int start = 0; start < total; start += bsm::kChunk)
    {
        const int n = std::min (bsm::kChunk, total - start);

        std::memcpy (cl, inL + start, (size_t) n * sizeof (float));
        std::memcpy (cr, inR + start, (size_t) n * sizeof (float));

        // ----------------------------------------------------------------- paramètres (1 fois par bloc de 64)
        inGainSm.setTargetValue (bsm::dbToLin (P (PID::in_gain)));
        outGainSm.setTargetValue (bsm::dbToLin (P (PID::out_gain)));
        mixSm.setTargetValue (P (PID::out_mix) * 0.01f);

        tgEq.setTargetValue    (P (PID::eq_on)    > 0.5f ? 1.f : 0.f);
        tgComp.setTargetValue  (P (PID::comp_on)  > 0.5f ? 1.f : 0.f);
        tgDeess.setTargetValue (P (PID::deess_on) > 0.5f ? 1.f : 0.f);
        tgSat.setTargetValue   (P (PID::sat_on)   > 0.5f ? 1.f : 0.f);
        tgDbl.setTargetValue   (P (PID::dbl_on)   > 0.5f ? 1.f : 0.f);
        tgDly.setTargetValue   (P (PID::dly_on)   > 0.5f ? 1.f : 0.f);
        tgRev.setTargetValue   (P (PID::rev_on)   > 0.5f ? 1.f : 0.f);

        hpf.setFrequency (P (PID::hpf_freq));
        gate.setThreshold (P (PID::gate_thresh));

        tune.setParams (P (PID::tune_on) > 0.5f, (int) P (PID::tune_key), (int) P (PID::tune_scale),
                        P (PID::tune_speed), P (PID::tune_amount) * 0.01f, P (PID::tune_shift));

        eq.setBand (0, P (PID::eq1_freq), P (PID::eq1_q), P (PID::eq1_gain));
        eq.setBand (1, P (PID::eq2_freq), P (PID::eq2_q), P (PID::eq2_gain));
        eq.setBand (2, P (PID::eq3_freq), P (PID::eq3_q), P (PID::eq3_gain));
        eq.setBand (3, P (PID::eq4_freq), P (PID::eq4_q), P (PID::eq4_gain));

        comp.setParams (P (PID::comp_thresh), P (PID::comp_ratio), P (PID::comp_attack),
                        P (PID::comp_release), P (PID::comp_makeup), P (PID::comp_mix) * 0.01f);

        deess.setParams (P (PID::deess_freq), P (PID::deess_thresh), P (PID::deess_amount) * 0.01f);

        sat.setParams ((int) P (PID::sat_type), P (PID::sat_drive), P (PID::sat_tone), P (PID::sat_mix) * 0.01f);

        dbl.setParams (P (PID::dbl_rate), P (PID::dbl_depth) * 0.01f, P (PID::dbl_mix) * 0.01f);

        {
            static constexpr float beats[7] = { 0.f, 2.f, 1.f, 1.5f, 0.5f, 1.f / 3.f, 0.25f };
            const int sync = juce::jlimit (0, 6, (int) P (PID::dly_sync));
            float timeMs = P (PID::dly_time);
            if (sync > 0)
                timeMs = juce::jlimit (5.f, 2000.f, (float) (60000.0 / bpm * beats[sync]));

            dly.setParams (timeMs, P (PID::dly_fb) * 0.01f, P (PID::dly_tone),
                           P (PID::dly_mix) * 0.01f, P (PID::dly_pingpong) > 0.5f);
        }

        rev.setParams (P (PID::rev_size) * 0.01f, P (PID::rev_damp) * 0.01f, P (PID::rev_width) * 0.01f,
                       P (PID::rev_predelay), P (PID::rev_lowcut), P (PID::rev_mix) * 0.01f);

        // ----------------------------------------------------------------- signal sec (compensé en latence)
        for (int i = 0; i < n; ++i)
        {
            peakIn = std::max ({ peakIn, std::abs (cl[i]), std::abs (cr[i]) });
            dryLine[0].push (cl[i]);  dryLine[1].push (cr[i]);
            dl[i] = dryLine[0].readInt (latency);
            dr[i] = dryLine[1].readInt (latency);
        }

        // ----------------------------------------------------------------- chaîne
        for (int i = 0; i < n; ++i)
        {
            const float g = inGainSm.getNextValue();
            cl[i] *= g;
            cr[i] *= g;
        }

        hpf.process (d, n);
        gate.process (d, n);
        tune.process (d, n);

        runToggle (tgEq,    d, n, [&] { eq.process (d, n); });
        runToggle (tgComp,  d, n, [&] { maxGR = std::max (maxGR, comp.process (d, n)); });
        runToggle (tgDeess, d, n, [&] { deess.process (d, n); });
        runToggle (tgSat,   d, n, [&] { sat.process (d, n); });
        runToggle (tgDbl,   d, n, [&] { dbl.process (d, n); });
        runToggle (tgDly,   d, n, [&] { dly.process (d, n); });
        runToggle (tgRev,   d, n, [&] { rev.process (d, n); });

        // ----------------------------------------------------------------- sortie : dry/wet, gain, clip de sécurité
        const bool clip = P (PID::out_clip) > 0.5f;
        for (int i = 0; i < n; ++i)
        {
            const float m = mixSm.getNextValue();
            const float og = outGainSm.getNextValue();

            float l = (dl[i] + (cl[i] - dl[i]) * m) * og;
            float r = (dr[i] + (cr[i] - dr[i]) * m) * og;

            if (clip) { l = bsm::softClip (l); r = bsm::softClip (r); }

            peakOut = std::max ({ peakOut, std::abs (l), std::abs (r) });

            if (outR != nullptr)
            {
                outL[start + i] = l;
                outR[start + i] = r;
            }
            else
                outL[start + i] = 0.5f * (l + r);
        }
    }

    // Sorties au-delà des 2 premiers canaux (ne devrait pas arriver) : silence
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, total);

    meterIn.store (std::max (meterIn.load(), peakIn));
    meterOut.store (std::max (meterOut.load(), peakOut));
    meterGR.store (std::max (meterGR.load(), maxGR));
}

//==============================================================================
int BasementAudioProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String BasementAudioProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void BasementAudioProcessor::applyPreset (int index)
{
    const auto& presets = getPresets();
    index = juce::jlimit (0, (int) presets.size() - 1, index);

    // IMPORTANT : toujours setValueNotifyingHost (et jamais setValue seul), sinon l'APVTS
    // ne met pas à jour ses valeurs "brutes" lues par le thread audio.

    // 1) tout remettre aux valeurs par défaut
    for (int i = 0; i < PID::Count; ++i)
        if (auto* p = apvts.getParameter (PID::name (i)))
            p->setValueNotifyingHost (p->getDefaultValue());

    // 2) appliquer les valeurs du preset
    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* p = apvts.getParameter (PID::name (id)))
            p->setValueNotifyingHost (p->convertTo0to1 (value));

    currentPreset = index;
}

//==============================================================================
void BasementAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BasementAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentPreset = juce::jlimit (0, (int) getPresets().size() - 1, (int) vt.getProperty ("preset", 0));
            apvts.replaceState (vt);
        }
}

//==============================================================================
juce::AudioProcessorEditor* BasementAudioProcessor::createEditor()
{
    return new BasementAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BasementAudioProcessor();
}

#pragma once

#include <JuceHeader.h>

//==============================================================================
//  Liste unique de tous les paramètres automatisables du plugin.
//  Pour en ajouter un : l'ajouter ici + dans createParameterLayout() ci-dessous.
//==============================================================================
#define BSM_PARAM_LIST(X)                                                                          \
    X(in_gain) X(hpf_freq) X(gate_thresh)                                                          \
    X(tune_on) X(tune_key) X(tune_scale) X(tune_speed) X(tune_amount) X(tune_shift)                \
    X(eq_on)                                                                                       \
    X(eq1_gain) X(eq1_freq) X(eq1_q)                                                               \
    X(eq2_gain) X(eq2_freq) X(eq2_q)                                                               \
    X(eq3_gain) X(eq3_freq) X(eq3_q)                                                               \
    X(eq4_gain) X(eq4_freq) X(eq4_q)                                                               \
    X(comp_on) X(comp_thresh) X(comp_ratio) X(comp_attack) X(comp_release) X(comp_makeup) X(comp_mix) \
    X(deess_on) X(deess_freq) X(deess_thresh) X(deess_amount)                                      \
    X(sat_on) X(sat_type) X(sat_drive) X(sat_tone) X(sat_mix)                                      \
    X(dbl_on) X(dbl_rate) X(dbl_depth) X(dbl_mix)                                                  \
    X(dly_on) X(dly_sync) X(dly_time) X(dly_fb) X(dly_tone) X(dly_mix) X(dly_pingpong)             \
    X(rev_on) X(rev_size) X(rev_damp) X(rev_width) X(rev_predelay) X(rev_lowcut) X(rev_mix)        \
    X(out_gain) X(out_mix) X(out_clip)

namespace PID
{
enum Index
{
#define X(n) n,
    BSM_PARAM_LIST(X)
#undef X
    Count
};

inline const char* name (int i)
{
    static const char* names[] = {
#define X(n) #n,
        BSM_PARAM_LIST(X)
#undef X
    };
    return names[i];
}
} // namespace PID

//==============================================================================
namespace fmt
{
inline juce::String db (float v)      { return juce::String (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
inline juce::String dbPlain (float v) { return juce::String (v, 1) + " dB"; }
inline juce::String hz (float v)      { return v >= 1000.f ? juce::String (v / 1000.f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }
inline juce::String hpf (float v)     { return v <= 20.5f ? juce::String ("OFF") : hz (v); }
inline juce::String ms (float v)      { return v < 10.f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms"; }
inline juce::String speed (float v)   { return v < 0.5f ? juce::String ("HARD") : ms (v); }
inline juce::String pct (float v)     { return juce::String (juce::roundToInt (v)) + " %"; }
inline juce::String ratio (float v)   { return juce::String (v, 1) + ":1"; }
inline juce::String st (float v)      { return juce::String (v > 0.005f ? "+" : "") + juce::String (v, 2) + " st"; }
inline juce::String q (float v)       { return juce::String (v, 2); }
inline juce::String gate (float v)    { return v < -79.5f ? juce::String ("OFF") : juce::String (juce::roundToInt (v)) + " dB"; }
inline juce::String rate (float v)    { return juce::String (v, 2) + " Hz"; }
} // namespace fmt

//==============================================================================
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    Layout layout;

    auto F = [&layout] (int id, const char* label, float lo, float hi, float def,
                        juce::String (*format) (float), float centre = 0.f, float step = 0.f)
    {
        juce::NormalisableRange<float> range (lo, hi, step);
        if (centre > 0.f)
            range.setSkewForCentre (centre);

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { PID::name (id), 1 }, label, range, def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([format] (float v, int) { return format (v); })));
    };

    auto B = [&layout] (int id, const char* label, bool def)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { PID::name (id), 1 }, label, def));
    };

    auto C = [&layout] (int id, const char* label, const juce::StringArray& items, int def = 0)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { PID::name (id), 1 }, label, items, def));
    };

    using namespace PID;

    // --- Input ---------------------------------------------------------------
    F (in_gain,     "Input Gain",   -24.f, 24.f, 0.f,   fmt::db, 0.f, 0.1f);
    F (hpf_freq,    "High-Pass",     20.f, 400.f, 20.f, fmt::hpf, 90.f);
    F (gate_thresh, "Gate",         -80.f, -20.f, -80.f, fmt::gate, 0.f, 1.f);

    // --- Tune ----------------------------------------------------------------
    B (tune_on, "Tune On", false);
    C (tune_key,   "Tune Key",   { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
    C (tune_scale, "Tune Scale", { "Chromatic", "Major", "Minor", "Pent Maj", "Pent Min", "Blues" }, 0);
    F (tune_speed,  "Tune Speed",  0.f, 400.f, 20.f, fmt::speed, 60.f);
    F (tune_amount, "Tune Amount", 0.f, 100.f, 100.f, fmt::pct, 0.f, 1.f);
    F (tune_shift,  "Tune Shift", -12.f, 12.f, 0.f, fmt::st, 0.f, 0.01f);

    // --- EQ ------------------------------------------------------------------
    B (eq_on, "EQ On", false);
    F (eq1_gain, "EQ Low Gain",  -15.f, 15.f, 0.f, fmt::db, 0.f, 0.1f);
    F (eq1_freq, "EQ Low Freq",   30.f, 400.f, 100.f, fmt::hz, 100.f);
    F (eq1_q,    "EQ Low Q",       0.3f, 2.f, 0.7f, fmt::q);
    F (eq2_gain, "EQ LowMid Gain", -15.f, 15.f, 0.f, fmt::db, 0.f, 0.1f);
    F (eq2_freq, "EQ LowMid Freq", 100.f, 1500.f, 320.f, fmt::hz, 400.f);
    F (eq2_q,    "EQ LowMid Q",    0.3f, 8.f, 1.f, fmt::q, 1.5f);
    F (eq3_gain, "EQ HiMid Gain",  -15.f, 15.f, 0.f, fmt::db, 0.f, 0.1f);
    F (eq3_freq, "EQ HiMid Freq",  800.f, 9000.f, 3000.f, fmt::hz, 3000.f);
    F (eq3_q,    "EQ HiMid Q",     0.3f, 8.f, 1.f, fmt::q, 1.5f);
    F (eq4_gain, "EQ Air Gain",    -15.f, 15.f, 0.f, fmt::db, 0.f, 0.1f);
    F (eq4_freq, "EQ Air Freq",    3000.f, 18000.f, 10000.f, fmt::hz, 9000.f);
    F (eq4_q,    "EQ Air Q",       0.3f, 2.f, 0.7f, fmt::q);

    // --- Compressor ----------------------------------------------------------
    B (comp_on, "Comp On", false);
    F (comp_thresh,  "Comp Threshold", -60.f, 0.f, -18.f, fmt::dbPlain, 0.f, 0.1f);
    F (comp_ratio,   "Comp Ratio",      1.f, 20.f, 4.f, fmt::ratio, 4.f, 0.1f);
    F (comp_attack,  "Comp Attack",     0.1f, 100.f, 10.f, fmt::ms, 10.f);
    F (comp_release, "Comp Release",    10.f, 1000.f, 120.f, fmt::ms, 120.f);
    F (comp_makeup,  "Comp Makeup",     0.f, 24.f, 0.f, fmt::dbPlain, 0.f, 0.1f);
    F (comp_mix,     "Comp Mix",        0.f, 100.f, 100.f, fmt::pct, 0.f, 1.f);

    // --- De-esser ------------------------------------------------------------
    B (deess_on, "DeEss On", false);
    F (deess_freq,   "DeEss Freq",      3000.f, 12000.f, 6500.f, fmt::hz, 6500.f);
    F (deess_thresh, "DeEss Threshold", -60.f, -5.f, -30.f, fmt::dbPlain, 0.f, 0.1f);
    F (deess_amount, "DeEss Amount",    0.f, 100.f, 50.f, fmt::pct, 0.f, 1.f);

    // --- Saturation ----------------------------------------------------------
    B (sat_on, "Sat On", false);
    C (sat_type, "Sat Type", { "Tape", "Tube", "Hard", "Fold" }, 0);
    F (sat_drive, "Sat Drive", 0.f, 36.f, 6.f, fmt::dbPlain, 0.f, 0.1f);
    F (sat_tone,  "Sat Tone", -12.f, 12.f, 0.f, fmt::db, 0.f, 0.1f);
    F (sat_mix,   "Sat Mix",   0.f, 100.f, 100.f, fmt::pct, 0.f, 1.f);

    // --- Doubler -------------------------------------------------------------
    B (dbl_on, "Double On", false);
    F (dbl_rate,  "Double Rate",  0.1f, 5.f, 0.6f, fmt::rate, 0.8f);
    F (dbl_depth, "Double Depth", 0.f, 100.f, 40.f, fmt::pct, 0.f, 1.f);
    F (dbl_mix,   "Double Mix",   0.f, 100.f, 30.f, fmt::pct, 0.f, 1.f);

    // --- Delay ---------------------------------------------------------------
    B (dly_on, "Delay On", false);
    C (dly_sync, "Delay Sync", { "Free", "1/2", "1/4", "1/4 D", "1/8", "1/8 T", "1/16" }, 0);
    F (dly_time, "Delay Time",     20.f, 1200.f, 300.f, fmt::ms, 300.f);
    F (dly_fb,   "Delay Feedback",  0.f, 95.f, 30.f, fmt::pct, 0.f, 1.f);
    F (dly_tone, "Delay Tone",    500.f, 16000.f, 6000.f, fmt::hz, 4000.f);
    F (dly_mix,  "Delay Mix",       0.f, 100.f, 15.f, fmt::pct, 0.f, 1.f);
    B (dly_pingpong, "Delay Ping-Pong", false);

    // --- Reverb --------------------------------------------------------------
    B (rev_on, "Reverb On", false);
    F (rev_size,     "Reverb Size",     0.f, 100.f, 50.f, fmt::pct, 0.f, 1.f);
    F (rev_damp,     "Reverb Damping",  0.f, 100.f, 50.f, fmt::pct, 0.f, 1.f);
    F (rev_width,    "Reverb Width",    0.f, 100.f, 100.f, fmt::pct, 0.f, 1.f);
    F (rev_predelay, "Reverb Pre-Delay", 0.f, 200.f, 20.f, fmt::ms, 40.f);
    F (rev_lowcut,   "Reverb Low Cut",  20.f, 1000.f, 250.f, fmt::hz, 200.f);
    F (rev_mix,      "Reverb Mix",      0.f, 100.f, 15.f, fmt::pct, 0.f, 1.f);

    // --- Output --------------------------------------------------------------
    F (out_gain, "Output Gain", -24.f, 12.f, 0.f, fmt::db, 0.f, 0.1f);
    F (out_mix,  "Dry/Wet",      0.f, 100.f, 100.f, fmt::pct, 0.f, 1.f);
    B (out_clip, "Safe Clip", true);

    return layout;
}

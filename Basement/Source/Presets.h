#pragma once

#include <JuceHeader.h>
#include "Params.h"

//==============================================================================
//  PRESETS D'USINE
//
//  ⚠️  Ce sont des chaînes "dans l'esprit" de chaque artiste, construites à partir
//  de l'esthétique générale de leur son — pas les vraies chaînes (elles ne sont pas publiques).
//  À affiner à l'oreille avec de vraies voix.
//
//  ⚠️  Juridique : avant toute commercialisation, remplace les noms d'artistes par des
//  noms de style (ou obtiens un accord écrit des artistes). Il suffit de modifier
//  `name` et `tagline` ci-dessous : rien d'autre dans le code ne dépend de ces textes.
//
//  Pour ajouter un artiste : copier un bloc { ... } et changer les valeurs.
//  Les paramètres non listés reviennent à leur valeur par défaut (voir Params.h).
//==============================================================================
struct Preset
{
    juce::String name;
    juce::String tagline;
    juce::uint32 accent;                              // couleur d'interface (ARGB)
    std::vector<std::pair<int, float>> values;        // { paramètre, valeur réelle }
};

inline const std::vector<Preset>& getPresets()
{
    using namespace PID;

    static const std::vector<Preset> presets = {

        // ------------------------------------------------------------------ 0
        { "INIT", "flat. clean. nothing yet.", 0xffe9e9ea, {} },

        // ------------------------------------------------------------------ 1
        //  Voix glossy / aérienne, autotune serré, brillance, doublage rêveur, espace large.
        { "P'TITE SOEUR", "glossy  /  airy  /  hard-tuned", 0xffff5fb0, {
            { hpf_freq, 110.f },

            { tune_on, 1.f }, { tune_key, 0 }, { tune_scale, 0 },
            { tune_speed, 6.f }, { tune_amount, 100.f }, { tune_shift, 0.f },

            { eq_on, 1.f },
            { eq1_gain, -2.5f }, { eq1_freq, 120.f },  { eq1_q, 0.7f },
            { eq2_gain, -3.f },  { eq2_freq, 320.f },  { eq2_q, 1.2f },
            { eq3_gain, 2.5f },  { eq3_freq, 3200.f }, { eq3_q, 0.9f },
            { eq4_gain, 5.f },   { eq4_freq, 11000.f },{ eq4_q, 0.7f },

            { comp_on, 1.f }, { comp_thresh, -22.f }, { comp_ratio, 4.f }, { comp_attack, 4.f },
            { comp_release, 90.f }, { comp_makeup, 4.f }, { comp_mix, 100.f },

            { deess_on, 1.f }, { deess_freq, 7500.f }, { deess_thresh, -34.f }, { deess_amount, 60.f },

            { sat_on, 1.f }, { sat_type, 0 }, { sat_drive, 5.f }, { sat_tone, 3.f }, { sat_mix, 45.f },

            { dbl_on, 1.f }, { dbl_rate, 0.45f }, { dbl_depth, 45.f }, { dbl_mix, 28.f },

            { dly_on, 1.f }, { dly_sync, 5 }, { dly_fb, 32.f }, { dly_tone, 6500.f },
            { dly_mix, 16.f }, { dly_pingpong, 1.f },

            { rev_on, 1.f }, { rev_size, 62.f }, { rev_damp, 40.f }, { rev_width, 100.f },
            { rev_predelay, 35.f }, { rev_lowcut, 300.f }, { rev_mix, 22.f },
        } },

        // ------------------------------------------------------------------ 2
        //  Voix écrasée, très saturée, collée au micro. Presque pas d'espace.
        { "SIM01", "crushed  /  saturated  /  in your face", 0xffff3b2f, {
            { hpf_freq, 95.f }, { gate_thresh, -60.f },

            { eq_on, 1.f },
            { eq1_gain, 2.f },   { eq1_freq, 110.f },  { eq1_q, 0.7f },
            { eq2_gain, -4.f },  { eq2_freq, 380.f },  { eq2_q, 1.5f },
            { eq3_gain, 4.5f },  { eq3_freq, 2600.f }, { eq3_q, 1.1f },
            { eq4_gain, 2.f },   { eq4_freq, 8500.f }, { eq4_q, 0.7f },

            { comp_on, 1.f }, { comp_thresh, -32.f }, { comp_ratio, 10.f }, { comp_attack, 1.5f },
            { comp_release, 55.f }, { comp_makeup, 9.f }, { comp_mix, 100.f },

            { deess_on, 1.f }, { deess_freq, 6800.f }, { deess_thresh, -30.f }, { deess_amount, 70.f },

            { sat_on, 1.f }, { sat_type, 2 }, { sat_drive, 16.f }, { sat_tone, 1.f }, { sat_mix, 75.f },

            { dly_on, 1.f }, { dly_sync, 0 }, { dly_time, 90.f }, { dly_fb, 18.f },
            { dly_tone, 4500.f }, { dly_mix, 10.f },

            { rev_on, 1.f }, { rev_size, 22.f }, { rev_damp, 65.f }, { rev_width, 70.f },
            { rev_predelay, 8.f }, { rev_lowcut, 400.f }, { rev_mix, 9.f },

            { out_gain, -2.f },
        } },

        // ------------------------------------------------------------------ 3
        //  Voix digitalisée, hyper-traitée, glitchée, mélancolique : tune à 0 ms, repli d'onde,
        //  doublage désaccordé, delay ping-pong et grande réverb sombre.
        { "55ASKY", "digital  /  glitched  /  melancholic", 0xffb98bff, {
            { hpf_freq, 140.f },

            { tune_on, 1.f }, { tune_key, 0 }, { tune_scale, 0 },
            { tune_speed, 0.f }, { tune_amount, 100.f }, { tune_shift, 0.f },

            { eq_on, 1.f },
            { eq1_gain, -4.f },  { eq1_freq, 150.f },   { eq1_q, 0.7f },
            { eq2_gain, -2.5f }, { eq2_freq, 250.f },   { eq2_q, 1.f },
            { eq3_gain, 3.f },   { eq3_freq, 4500.f },  { eq3_q, 0.8f },
            { eq4_gain, 6.f },   { eq4_freq, 12000.f }, { eq4_q, 0.7f },

            { comp_on, 1.f }, { comp_thresh, -20.f }, { comp_ratio, 6.f }, { comp_attack, 2.f },
            { comp_release, 70.f }, { comp_makeup, 5.f }, { comp_mix, 100.f },

            { deess_on, 1.f }, { deess_freq, 8000.f }, { deess_thresh, -32.f }, { deess_amount, 70.f },

            { sat_on, 1.f }, { sat_type, 3 }, { sat_drive, 10.f }, { sat_tone, 4.f }, { sat_mix, 40.f },

            { dbl_on, 1.f }, { dbl_rate, 1.2f }, { dbl_depth, 70.f }, { dbl_mix, 40.f },

            { dly_on, 1.f }, { dly_sync, 4 }, { dly_fb, 45.f }, { dly_tone, 4000.f },
            { dly_mix, 22.f }, { dly_pingpong, 1.f },

            { rev_on, 1.f }, { rev_size, 80.f }, { rev_damp, 55.f }, { rev_width, 100.f },
            { rev_predelay, 50.f }, { rev_lowcut, 350.f }, { rev_mix, 30.f },
        } },
    };

    return presets;
}

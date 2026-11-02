//==============================================================================
//  Banc de test BASEMENT
//    BasementTests                 → lance tous les tests audio (sans oreilles : mesures objectives)
//    BasementTests --snapshot DIR  → exporte une capture PNG de l'interface pour chaque preset
//==============================================================================
#include <JuceHeader.h>
#include <cstdio>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static int failures = 0;

#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        const bool ok_ = (cond);                                              \
        if (! ok_) ++failures;                                                \
        std::printf ("  %s ", ok_ ? "ok  " : "FAIL");                         \
        std::printf (__VA_ARGS__);                                            \
        std::printf ("\n");                                                   \
    } while (0)

//------------------------------------------------------------------------------
struct Rig
{
    explicit Rig (double sampleRate = 48000.0, int block = 512, int numIn = 2, int numOut = 2)
        : sr (sampleRate), blockSize (block), nIn (numIn), nOut (numOut)
    {
        proc.applyPreset (0);   // INIT (avant prepareToPlay : les lissages démarrent sur les bonnes valeurs)
        proc.setPlayConfigDetails (nIn, nOut, sr, blockSize);
        proc.prepareToPlay (sr, blockSize);
    }

    void set (int id, float v)
    {
        auto* p = proc.apvts.getParameter (PID::name (id));
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    float get (int id) const { return proc.apvts.getRawParameterValue (PID::name (id))->load(); }

    void run (std::vector<float>& L, std::vector<float>& R)
    {
        juce::MidiBuffer midi;
        const int total = (int) L.size();
        for (int pos = 0; pos < total; pos += blockSize)
        {
            const int n = std::min (blockSize, total - pos);
            juce::AudioBuffer<float> buf (std::max (nIn, nOut), n);
            buf.clear();
            buf.copyFrom (0, 0, L.data() + pos, n);
            if (nIn > 1) buf.copyFrom (1, 0, R.data() + pos, n);
            proc.processBlock (buf, midi);
            std::memcpy (L.data() + pos, buf.getReadPointer (0), (size_t) n * sizeof (float));
            if (nOut > 1) std::memcpy (R.data() + pos, buf.getReadPointer (1), (size_t) n * sizeof (float));
            else          std::memcpy (R.data() + pos, buf.getReadPointer (0), (size_t) n * sizeof (float));
        }
    }

    BasementAudioProcessor proc;
    double sr;
    int blockSize, nIn, nOut;
};

//------------------------------------------------------------------------------
static double rmsDb (const std::vector<float>& x, int from, int to)
{
    double s = 0;
    for (int i = from; i < to; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
    return 10.0 * std::log10 (s / std::max (1, to - from) + 1e-20);
}

/** Fréquence fondamentale par autocorrélation normalisée (plus petit lag proche du max). */
static double estimateFreq (const std::vector<float>& x, int start, int len, double sr, double fmin = 80.0, double fmax = 900.0)
{
    const int lagMin = (int) (sr / fmax), lagMax = (int) (sr / fmin);
    std::vector<double> r ((size_t) lagMax + 2, 0.0);
    double best = -1;
    for (int lag = lagMin; lag <= lagMax; ++lag)
    {
        double xy = 0, xx = 0, yy = 0;
        for (int i = 0; i < len; ++i)
        {
            const double a = x[(size_t) (start + i)], b = x[(size_t) (start + i + lag)];
            xy += a * b; xx += a * a; yy += b * b;
        }
        r[(size_t) lag] = xy / std::sqrt (xx * yy + 1e-20);
        best = std::max (best, r[(size_t) lag]);
    }
    for (int lag = lagMin + 1; lag < lagMax; ++lag)
        if (r[(size_t) lag] >= 0.97 * best && r[(size_t) lag] >= r[(size_t) lag - 1] && r[(size_t) lag] >= r[(size_t) lag + 1])
        {
            const double a = r[(size_t) lag - 1], b = r[(size_t) lag], c = r[(size_t) lag + 1];
            const double d = 2 * (2 * b - a - c);
            const double refined = lag + (std::abs (d) > 1e-12 ? (c - a) / d : 0.0);
            return sr / refined;
        }
    return 0.0;
}

static std::vector<float> voiceLike (double freq, double sr, double seconds, float amp)
{
    std::vector<float> x ((size_t) (seconds * sr));
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double t = (double) i / sr, w = juce::MathConstants<double>::twoPi * freq * t;
        x[i] = amp * (float) (std::sin (w) + 0.5 * std::sin (2 * w) + 0.3 * std::sin (3 * w) + 0.15 * std::sin (4 * w)) / 1.95f;
    }
    return x;
}

//------------------------------------------------------------------------------
static void testPassthrough()
{
    std::printf ("\n[1] INIT = transparent (latence compensée)\n");
    Rig rig;
    juce::Random rng (7);
    std::vector<float> L (48000), R (48000);
    for (size_t i = 0; i < L.size(); ++i) { L[i] = 0.5f * (rng.nextFloat() * 2 - 1); R[i] = 0.5f * (rng.nextFloat() * 2 - 1); }
    auto L0 = L, R0 = R;
    rig.run (L, R);

    const int lat = rig.proc.getLatencySamples();
    double maxDiff = 0;
    for (int i = lat; i < (int) L.size(); ++i)
        maxDiff = std::max ({ maxDiff, (double) std::abs (L[(size_t) i] - L0[(size_t) (i - lat)]), (double) std::abs (R[(size_t) i] - R0[(size_t) (i - lat)]) });

    CHECK (lat == 600, "latence reportée = %d échantillons (%.1f ms)", lat, lat * 1000.0 / 48000.0);
    CHECK (maxDiff < 1e-5, "sortie == entrée retardée (écart max %.2e)", maxDiff);
}

static void testTune()
{
    std::printf ("\n[2] AutoTune : fréquence mesurée en sortie\n");
    struct Case { double in, expected; const char* what; };
    const Case cases[] = { { 224.0, 220.00, "224 Hz (A3 +31c)  → A3" },
                           { 237.0, 233.08, "237 Hz (A#3 +29c) → A#3" },
                           { 205.0, 207.65, "205 Hz (G#3 -22c) → G#3" },
                           { 110.0 * std::pow (2.0, 0.36 / 12.0), 110.0, "A2 +36c → A2 (voix grave)" },
                           { 392.0 * std::pow (2.0, 0.30 / 12.0), 392.0, "G4 +30c → G4 (voix aiguë)" } };

    for (const auto& c : cases)
    {
        Rig rig;
        rig.set (PID::tune_on, 1); rig.set (PID::tune_scale, 0); rig.set (PID::tune_speed, 0); rig.set (PID::tune_amount, 100);
        auto L = voiceLike (c.in, 48000, 3.0, 0.3f), R = L;
        rig.run (L, R);
        const double f = estimateFreq (L, 48000 * 2, 12000, 48000, c.in * 0.6, c.in * 1.5);
        const double centsErr = 1200.0 * std::log2 (f / c.expected);
        CHECK (std::abs (centsErr) < 6.0, "%s → mesuré %.2f Hz (écart %.1f cents)", c.what, f, centsErr);
    }

    {   // gamme mineure de A : un Do# (A mineur n'en a pas) doit aller à C ou D
        Rig rig;
        rig.set (PID::tune_on, 1); rig.set (PID::tune_key, 9); rig.set (PID::tune_scale, 2); rig.set (PID::tune_speed, 0);
        const double cs3 = 277.18;                       // C#4
        auto L = voiceLike (cs3, 48000, 3.0, 0.3f), R = L;
        rig.run (L, R);
        const double f = estimateFreq (L, 48000 * 2, 12000, 48000, 200, 400);
        const bool toC = std::abs (1200 * std::log2 (f / 261.63)) < 8, toD = std::abs (1200 * std::log2 (f / 293.66)) < 8;
        CHECK (toC || toD, "A mineur : C#4 (277 Hz) → %.2f Hz (C4 ou D4 attendu)", f);
    }

    for (double semis : { 12.0, 7.0, 3.0, -5.0, -12.0 })
    {   // transposition seule (correction à 0 %) : hauteur exacte + pas de creux d'amplitude
        Rig rig;
        rig.set (PID::tune_on, 1); rig.set (PID::tune_amount, 0); rig.set (PID::tune_shift, (float) semis);
        auto L = voiceLike (224.0, 48000, 3.0, 0.3f), R = L;
        rig.run (L, R);
        const double expected = 224.0 * std::pow (2.0, semis / 12.0);
        const double f = estimateFreq (L, 48000 * 2, 12000, 48000, expected * 0.7, expected * 1.4);

        // enveloppe : RMS glissant sur 2 périodes, rapport min/max sur la dernière seconde
        const int win = (int) (48000.0 / expected * 2.0);
        double mn = 1e9, mx = 0;
        for (int i = 48000 * 2; i < 48000 * 3 - win; i += 64)
        {
            double e = 0;
            for (int j = 0; j < win; ++j) e += (double) L[(size_t) (i + j)] * L[(size_t) (i + j)];
            e = std::sqrt (e / win);
            mn = std::min (mn, e); mx = std::max (mx, e);
        }
        CHECK (std::abs (1200 * std::log2 (f / expected)) < 10.0 && mn / mx > 0.8,
               "SHIFT %+.0f st : 224 Hz → %.2f Hz (attendu %.2f), creux d'amplitude min/max = %.2f", semis, f, expected, mn / mx);
    }

    {   // silence / bruit : pas de pitch → pas de correction → sortie saine
        Rig rig;
        rig.set (PID::tune_on, 1); rig.set (PID::tune_speed, 0);
        juce::Random rng (3);
        std::vector<float> L (48000), R;
        for (auto& s : L) s = 0.1f * (rng.nextFloat() * 2 - 1);
        R = L;
        rig.run (L, R);
        bool finite = true;
        for (auto s : L) finite &= std::isfinite (s);
        CHECK (finite, "bruit blanc : sortie finie");
    }
}

static void testCompressor()
{
    std::printf ("\n[3] Compresseur : courbe statique\n");
    Rig rig;
    rig.set (PID::comp_on, 1); rig.set (PID::comp_thresh, -30); rig.set (PID::comp_ratio, 10);
    rig.set (PID::comp_attack, 0.1f); rig.set (PID::comp_release, 100); rig.set (PID::comp_makeup, 0); rig.set (PID::comp_mix, 100);

    std::vector<float> L (48000), R;
    for (size_t i = 0; i < L.size(); ++i) L[i] = 0.316f * (float) std::sin (2 * juce::MathConstants<double>::pi * 1000.0 * (double) i / 48000.0);
    R = L;
    rig.run (L, R);

    const double reduction = rmsDb (L, 600 + 40000, 600 + 47000) - (20.0 * std::log10 (0.316 / std::sqrt (2.0)) );
    CHECK (std::abs (reduction + 18.0) < 1.5, "-10 dBFS, seuil -30, 10:1 → réduction mesurée %.1f dB (attendu -18)", reduction);
    CHECK (rig.proc.meterGR.load() > 15.0f, "compteur GR = %.1f dB", rig.proc.meterGR.load());
}

static void testDelay()
{
    std::printf ("\n[4] Delay : position de l'écho\n");
    Rig rig;
    rig.set (PID::dly_on, 1); rig.set (PID::dly_sync, 0); rig.set (PID::dly_time, 250);
    rig.set (PID::dly_fb, 0); rig.set (PID::dly_mix, 100); rig.set (PID::dly_tone, 16000);

    std::vector<float> L (48000 * 3, 0.f), R;
    L[48000 * 2] = 0.5f;           // impulsion après 2 s (le temps de retard a fini de glisser)
    R = L;
    rig.run (L, R);

    int echo = -1; float peak = 0;
    for (int i = 48000 * 2 + 600 + 200; i < (int) L.size(); ++i)
        if (std::abs (L[(size_t) i]) > peak) { peak = std::abs (L[(size_t) i]); echo = i; }

    const int expected = 48000 * 2 + 600 + 12000;
    CHECK (std::abs (echo - expected) <= 3, "écho à %d (attendu %d), amplitude %.3f", echo, expected, peak);
}

static void testReverb()
{
    std::printf ("\n[5] Reverb : niveau et queue\n");
    Rig rig;
    rig.set (PID::rev_on, 1); rig.set (PID::rev_size, 50); rig.set (PID::rev_mix, 100); rig.set (PID::rev_predelay, 0); rig.set (PID::rev_lowcut, 20);

    juce::Random rng (11);
    std::vector<float> L (48000 * 4, 0.f), R;
    for (int i = 0; i < 24000; ++i) L[(size_t) i] = 0.1f * (rng.nextFloat() * 2 - 1);
    auto ref = L;
    R = L;
    rig.run (L, R);

    const double inDb = rmsDb (ref, 12000, 24000);
    const double wetDb = rmsDb (L, 12000 + 600, 24000 + 600) ;
    const double tailDb = rmsDb (L, 48000 + 600, 48000 + 12600);
    std::printf ("      entrée %.1f dB | sortie pendant le bruit %.1f dB | queue à +1 s %.1f dB\n", inDb, wetDb, tailDb);
    CHECK (wetDb - inDb < 9.0 && wetDb - inDb > -1.0, "niveau wet+dry cohérent (%.1f dB vs entrée)", wetDb - inDb);
    CHECK (tailDb > inDb - 45.0 && tailDb < inDb, "queue audible et décroissante");
}

static void testAllPresets()
{
    std::printf ("\n[6] Tous les presets : stabilité sur signal de voix\n");
    const auto& presets = getPresets();
    juce::Random rng (5);

    // "voix" : glissando 150→260 Hz avec vibrato + enveloppe syllabique + souffle
    const double sr = 48000.0;
    std::vector<float> src ((size_t) (sr * 4.0));
    double ph = 0;
    for (size_t i = 0; i < src.size(); ++i)
    {
        const double t = (double) i / sr;
        const double f = 150.0 + 110.0 * (t / 4.0) + 4.0 * std::sin (2 * juce::MathConstants<double>::pi * 5.5 * t);
        ph += juce::MathConstants<double>::twoPi * f / sr;
        const double env = 0.5 + 0.5 * std::sin (2 * juce::MathConstants<double>::pi * 2.2 * t);
        const double v = std::sin (ph) + 0.6 * std::sin (2 * ph) + 0.4 * std::sin (3 * ph) + 0.25 * std::sin (5 * ph);
        src[i] = (float) (0.22 * env * v + 0.01 * (rng.nextFloat() * 2 - 1));
    }

    for (int b : { 37, 480, 1024 })
        for (size_t p = 0; p < presets.size(); ++p)
        {
            Rig rig (sr, b);
            rig.proc.applyPreset ((int) p);
            auto L = src, R = src;
            rig.run (L, R);

            bool finite = true; float peak = 0;
            for (size_t i = 0; i < L.size(); ++i) { finite &= std::isfinite (L[i]) && std::isfinite (R[i]); peak = std::max ({ peak, std::abs (L[i]), std::abs (R[i]) }); }
            CHECK (finite && peak <= 1.0f, "block %4d  %-13s  in %.1f dB → out %.1f dB, crête %.3f, GR max %.1f dB",
                   b, presets[p].name.toRawUTF8(), rmsDb (src, 0, (int) src.size()), rmsDb (L, 0, (int) L.size()), peak, rig.proc.meterGR.load());
        }
}

static void testPresetRoundTrip()
{
    std::printf ("\n[7] Presets : les valeurs sont bien appliquées\n");
    Rig rig;
    const auto& presets = getPresets();
    int checked = 0, bad = 0;
    for (size_t p = 0; p < presets.size(); ++p)
    {
        rig.proc.applyPreset ((int) p);
        for (const auto& [id, v] : presets[p].values)
        {
            ++checked;
            if (std::abs (rig.get (id) - v) > 0.02f * std::max (1.f, std::abs (v)))
            {
                ++bad;
                std::printf ("      %s / %s : attendu %.3f, lu %.3f\n", presets[p].name.toRawUTF8(), PID::name (id), v, rig.get (id));
            }
        }
    }
    CHECK (bad == 0, "%d valeurs de preset vérifiées, %d erreurs", checked, bad);

    // état sérialisé
    rig.proc.applyPreset (3);
    juce::MemoryBlock mb;
    rig.proc.getStateInformation (mb);
    Rig other;
    other.proc.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (other.proc.getCurrentProgram() == 3 && std::abs (other.get (PID::tune_speed) - 0.f) < 0.01f, "sauvegarde/restauration d'état (preset %d)", other.proc.getCurrentProgram());
}

static void testLayouts()
{
    std::printf ("\n[8] Mono / stéréo\n");
    for (auto [i, o] : { std::pair<int, int> { 1, 1 }, { 1, 2 }, { 2, 2 }, { 2, 1 } })
    {
        Rig rig (44100.0, 256, i, o);
        rig.proc.applyPreset (1);
        auto L = voiceLike (200.0, 44100, 1.0, 0.3f), R = L;
        rig.run (L, R);
        bool finite = true;
        for (auto s : L) finite &= std::isfinite (s);
        CHECK (finite, "%d in → %d out, 44.1 kHz : sortie finie, RMS %.1f dB", i, o, rmsDb (L, 0, (int) L.size()));
    }
    for (double sr : { 44100.0, 88200.0, 96000.0, 192000.0 })
    {
        Rig rig (sr, 512);
        rig.proc.applyPreset (3);
        auto L = voiceLike (200.0, sr, 1.0, 0.3f), R = L;
        rig.run (L, R);
        bool finite = true;
        for (auto s : L) finite &= std::isfinite (s);
        CHECK (finite, "preset 55ASKY à %.0f Hz : sortie finie, latence %d éch.", sr, rig.proc.getLatencySamples());
    }
}

//------------------------------------------------------------------------------
static void snapshots (const juce::File& dir)
{
    dir.createDirectory();
    const auto& presets = getPresets();
    for (size_t p = 0; p < presets.size(); ++p)
    {
        BasementAudioProcessor proc;
        proc.applyPreset ((int) p);
        proc.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (BasementUI::kWidth, BasementUI::kHeight);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);

        auto file = dir.getChildFile ("preset" + juce::String ((int) p) + ".png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat png;
        png.writeImageToStream (img, out);
        std::printf ("  écrit %s\n", file.getFullPathName().toRawUTF8());
    }
}

//------------------------------------------------------------------------------
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc >= 3 && juce::String (argv[1]) == "--snapshot")
    {
        snapshots (juce::File (argv[2]));
        return 0;
    }

    testPassthrough();
    testTune();
    testCompressor();
    testDelay();
    testReverb();
    testAllPresets();
    testPresetRoundTrip();
    testLayouts();

    std::printf ("\n==== %s (%d échec%s) ====\n", failures == 0 ? "TOUT EST OK" : "ÉCHECS", failures, failures > 1 ? "s" : "");
    return failures == 0 ? 0 : 1;
}

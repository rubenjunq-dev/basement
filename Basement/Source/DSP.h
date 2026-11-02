#pragma once

//==============================================================================
//  BASEMENT — moteur audio
//  Tous les modules travaillent par petits blocs (kChunk échantillons) sur 2 canaux.
//  Aucune allocation dans le thread audio : tout est alloué dans prepare().
//==============================================================================

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

namespace bsm
{
constexpr int kChunk = 64;
constexpr double kTwoPi = 6.283185307179586;

inline float dbToLin (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float linToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

/** Coefficient de lissage "one-pole" pour une constante de temps en ms (1 = instantané). */
inline float coefFromMs (double sr, float ms) noexcept
{
    return ms <= 0.01f ? 1.0f : 1.0f - std::exp (-1.0f / (0.001f * ms * (float) sr));
}

//==============================================================================
//  Biquad (RBJ cookbook), état en double, 2 canaux
//==============================================================================
struct Biquad
{
    enum class Type { LowPass, HighPass, BandPass, Peak, LowShelf, HighShelf };

    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] = { 0, 0 }, z2[2] = { 0, 0 };

    void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0.0; }

    void set (Type t, double sr, double freq, double q, double gainDb = 0.0) noexcept
    {
        freq = juce::jlimit (10.0, sr * 0.49, freq);
        q = std::max (0.05, q);

        const double w0 = kTwoPi * freq / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        const double A = std::pow (10.0, gainDb / 40.0);

        double nb0 = 1, nb1 = 0, nb2 = 0, na0 = 1, na1 = 0, na2 = 0;

        switch (t)
        {
            case Type::LowPass:
                nb0 = (1 - cw) / 2; nb1 = 1 - cw; nb2 = (1 - cw) / 2;
                na0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha;
                break;
            case Type::HighPass:
                nb0 = (1 + cw) / 2; nb1 = -(1 + cw); nb2 = (1 + cw) / 2;
                na0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha;
                break;
            case Type::BandPass:
                nb0 = alpha; nb1 = 0; nb2 = -alpha;
                na0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha;
                break;
            case Type::Peak:
                nb0 = 1 + alpha * A; nb1 = -2 * cw; nb2 = 1 - alpha * A;
                na0 = 1 + alpha / A; na1 = -2 * cw; na2 = 1 - alpha / A;
                break;
            case Type::LowShelf:
            {
                const double s = 2.0 * std::sqrt (A) * alpha;
                nb0 = A * ((A + 1) - (A - 1) * cw + s);
                nb1 = 2 * A * ((A - 1) - (A + 1) * cw);
                nb2 = A * ((A + 1) - (A - 1) * cw - s);
                na0 = (A + 1) + (A - 1) * cw + s;
                na1 = -2 * ((A - 1) + (A + 1) * cw);
                na2 = (A + 1) + (A - 1) * cw - s;
                break;
            }
            case Type::HighShelf:
            {
                const double s = 2.0 * std::sqrt (A) * alpha;
                nb0 = A * ((A + 1) + (A - 1) * cw + s);
                nb1 = -2 * A * ((A - 1) + (A + 1) * cw);
                nb2 = A * ((A + 1) + (A - 1) * cw - s);
                na0 = (A + 1) - (A - 1) * cw + s;
                na1 = 2 * ((A - 1) - (A + 1) * cw);
                na2 = (A + 1) - (A - 1) * cw - s;
                break;
            }
        }

        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0;
        a1 = na1 / na0; a2 = na2 / na0;
    }

    inline float process (float x, int ch) noexcept
    {
        const double in = x;
        const double y = b0 * in + z1[ch];
        z1[ch] = b1 * in - a1 * y + z2[ch];
        z2[ch] = b2 * in - a2 * y;
        return (float) y;
    }

    /** Réponse en amplitude (dB) à la fréquence f — utilisé pour dessiner la courbe d'EQ. */
    double magnitudeDb (double f, double sr) const noexcept
    {
        const double w = kTwoPi * f / sr;
        const double c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = b1 * s1 + b2 * s2;
        const double dr = 1 + a1 * c1 + a2 * c2,  di = a1 * s1 + a2 * s2;
        return 10.0 * std::log10 ((nr * nr + ni * ni + 1e-30) / (dr * dr + di * di + 1e-30));
    }
};

/** Configuration commune (DSP + dessin de la courbe) des 4 bandes d'EQ. */
inline void configureEqBand (Biquad& bq, int band, double sr, float freq, float q, float gainDb) noexcept
{
    static constexpr Biquad::Type types[4] = { Biquad::Type::LowShelf, Biquad::Type::Peak,
                                               Biquad::Type::Peak, Biquad::Type::HighShelf };
    bq.set (types[juce::jlimit (0, 3, band)], sr, freq, q, gainDb);
}

//==============================================================================
//  Ligne à retard (puissance de 2, interpolation linéaire / Hermite)
//  Convention : après push(x), readInt(0) == x.
//==============================================================================
inline float hermite4 (float xm1, float x0, float x1, float x2, float t) noexcept
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, wp = 0;

    void prepare (int maxDelaySamples)
    {
        const int n = juce::nextPowerOfTwo (maxDelaySamples + 8);
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        wp = 0;
    }

    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); wp = 0; }

    inline void push (float x) noexcept { buf[(size_t) wp] = x; wp = (wp + 1) & mask; }
    inline float readInt (int d) const noexcept { return buf[(size_t) ((wp - 1 - d) & mask)]; }

    inline float readLinear (float d) const noexcept
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = readInt (i), b = readInt (i + 1);
        return a + (b - a) * f;
    }

    inline float readHermite (float d) const noexcept
    {
        const int i = (int) d;
        const float f = d - (float) i;
        return hermite4 (readInt (i + 2), readInt (i + 1), readInt (i), readInt (i - 1), 1.0f - f);
    }
};

//==============================================================================
//  High-pass 24 dB/oct (2 biquads Butterworth)
//==============================================================================
class HighPass24
{
public:
    void prepare (double sr) { sampleRate = sr; a.reset(); b.reset(); }

    void setFrequency (float f)
    {
        active = f > 20.5f;
        if (active)
        {
            a.set (Biquad::Type::HighPass, sampleRate, f, 0.5412);
            b.set (Biquad::Type::HighPass, sampleRate, f, 1.3066);
        }
    }

    void process (float* const* d, int n) noexcept
    {
        if (! active)
        {
            a.reset(); b.reset();
            return;
        }
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                d[ch][i] = b.process (a.process (d[ch][i], ch), ch);
    }

private:
    double sampleRate = 44100.0;
    Biquad a, b;
    bool active = false;
};

//==============================================================================
//  Noise gate (liée stéréo, avec hystérésis)
//==============================================================================
class Gate
{
public:
    void prepare (double sr)
    {
        envAtk = coefFromMs (sr, 1.f);   envRel = coefFromMs (sr, 40.f);
        gAtk   = coefFromMs (sr, 3.f);   gRel   = coefFromMs (sr, 140.f);
        env = 0.f; gain = 1.f; open = true;
    }

    void setThreshold (float db) { threshDb = db; enabled = db > -79.5f; }

    void process (float* const* d, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float lvl = std::max (std::abs (d[0][i]), std::abs (d[1][i]));
            env += (lvl - env) * (lvl > env ? envAtk : envRel);

            if (enabled)
            {
                const float db = linToDb (env);
                if (! open && db > threshDb)            open = true;
                else if (open && db < threshDb - 4.0f)  open = false;
            }
            else
                open = true;

            const float target = open ? 1.0f : 0.04f;
            gain += (target - gain) * (target > gain ? gAtk : gRel);
            d[0][i] *= gain;
            d[1][i] *= gain;
        }
    }

private:
    float envAtk = 0.1f, envRel = 0.001f, gAtk = 0.1f, gRel = 0.001f;
    float env = 0.f, gain = 1.f, threshDb = -80.f;
    bool open = true, enabled = false;
};

//==============================================================================
//  Détecteur de pitch (YIN simplifié, signal décimé à ~12 kHz)
//==============================================================================
class PitchDetector
{
public:
    void prepare (double sr)
    {
        decim   = std::max (1, (int) std::lround (sr / 12000.0));
        dsr     = sr / decim;
        tauMin  = std::max (2, (int) std::floor (dsr / 1000.0));
        tauMax  = (int) std::ceil (dsr / 70.0);
        W       = (int) std::lround (dsr * 0.032);
        frameLen = W + tauMax + 2;
        hop     = std::max (16, (int) std::lround (dsr * 0.006));
        ringSize = juce::nextPowerOfTwo (frameLen + 8);
        ring.assign ((size_t) ringSize, 0.f);
        work.assign ((size_t) frameLen, 0.f);
        dif.assign ((size_t) tauMax + 2, 0.f);
        cmnd.assign ((size_t) tauMax + 2, 1.f);
        reset();
    }

    void reset() noexcept
    {
        wr = 0; filled = 0; sinceAnalysis = 0; acc = 0.f; accCount = 0; freq = 0.f;
    }

    /** Pousse un échantillon (taux plein). Renvoie true quand une nouvelle estimation est dispo. */
    bool push (float x) noexcept
    {
        acc += x;
        if (++accCount < decim)
            return false;

        const float v = acc / (float) decim;
        acc = 0.f; accCount = 0;

        ring[(size_t) wr] = v;
        wr = (wr + 1) & (ringSize - 1);
        filled = std::min (filled + 1, ringSize);

        if (++sinceAnalysis < hop || filled < frameLen)
            return false;

        sinceAnalysis = 0;
        analyse();
        return true;
    }

    float getFrequency() const noexcept { return freq; }   // 0 = pas de voix détectée
    int hopInInputSamples() const noexcept { return hop * decim; }

private:
    void analyse() noexcept
    {
        const int start = (wr - frameLen) & (ringSize - 1);
        for (int k = 0; k < frameLen; ++k)
            work[(size_t) k] = ring[(size_t) ((start + k) & (ringSize - 1))];

        float energy = 0.f;
        for (int k = 0; k < W; ++k)
            energy += work[(size_t) k] * work[(size_t) k];

        if (std::sqrt (energy / (float) W) < 0.002f)   // ≈ -54 dBFS
        {
            freq = 0.f;
            return;
        }

        const float* x = work.data();
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0.f;
            for (int j = 0; j < W; ++j)
            {
                const float diff = x[j] - x[j + tau];
                s += diff * diff;
            }
            dif[(size_t) tau] = s;
        }

        cmnd[0] = 1.f;
        float running = 0.f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += dif[(size_t) tau];
            cmnd[(size_t) tau] = running > 1.0e-12f ? dif[(size_t) tau] * (float) tau / running : 1.f;
        }

        int best = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < 0.15f)
            {
                while (tau + 1 <= tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }

        if (best < 0)
        {
            int m = tauMin;
            for (int tau = tauMin; tau <= tauMax; ++tau)
                if (cmnd[(size_t) tau] < cmnd[(size_t) m])
                    m = tau;
            if (cmnd[(size_t) m] < 0.35f)
                best = m;
        }

        if (best < 0)
        {
            freq = 0.f;
            return;
        }

        float refined = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float s0 = cmnd[(size_t) best - 1], s1 = cmnd[(size_t) best], s2 = cmnd[(size_t) best + 1];
            const float denom = 2.f * (2.f * s1 - s2 - s0);
            if (std::abs (denom) > 1.0e-9f)
                refined += juce::jlimit (-1.f, 1.f, (s2 - s0) / denom);
        }

        const float f = (float) dsr / refined;
        freq = (f > 55.f && f < 1200.f) ? f : 0.f;
    }

    double dsr = 12000.0;
    int decim = 4, tauMin = 12, tauMax = 171, W = 384, frameLen = 560, hop = 72, ringSize = 1024;
    int wr = 0, filled = 0, sinceAnalysis = 0, accCount = 0;
    float acc = 0.f, freq = 0.f;
    std::vector<float> ring, work, dif, cmnd;
};

//==============================================================================
//  AutoTune : détection YIN → note la plus proche dans la gamme → pitch-shift
//  par double ligne à retard fenêtrée (Hann), ~25 ms. Latence reportée = 12.5 ms.
//
//  Les deux "têtes de lecture" sont espacées d'un NOMBRE ENTIER de périodes de la voix
//  (re-calculé à chaque rebouclage de tête) : elles s'additionnent donc en phase, ce qui
//  donne une hauteur exacte et évite les creux d'amplitude du pitch-shifter naïf.
//
//  Le chemin "sec" est toujours retardé de la même latence, que le module soit
//  actif ou non : la latence reportée à la DAW ne change donc jamais.
//==============================================================================
class AutoTune
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        halfWin = (int) std::lround (0.0125 * sr);
        winLen  = 2 * halfWin;
        for (auto& l : lines) l.prepare (2 * winLen + 16);

        detector.prepare (sr);
        hopSamples = detector.hopInInputSamples();
        holdHops = std::max (4, (int) std::lround (0.06 * sr / (double) hopSamples));

        resetHeads (1.f);
        periodEst = 0.f;
        corr = corrTarget = 0.f;
        shiftSm = shiftTarget = 0.f;
        onSmooth = 0.f;
        unvoicedHops = 1000; prevNote = -1; histCount = 0; histPos = 0;
        detMidi.store (-1.f); tgtMidi.store (-1.f);
        onCoef = coefFromMs (sr, 8.f);
        shiftCoef = coefFromMs (sr, 25.f);
    }

    int getLatencySamples() const noexcept { return halfWin; }
    float getDetectedMidi() const noexcept { return detMidi.load(); }
    float getTargetMidi() const noexcept   { return tgtMidi.load(); }

    void setParams (bool on, int key, int scaleIdx, float speedMs, float amount01, float shiftSemitones)
    {
        tuneOn = on;
        keyIdx = juce::jlimit (0, 11, key);
        scale  = juce::jlimit (0, 5, scaleIdx);
        corrCoef = coefFromMs (sampleRate, speedMs);
        amount = amount01;
        shiftTarget = shiftSemitones;
    }

    void process (float* const* d, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float inL = d[0][i], inR = d[1][i];
            lines[0].push (inL);
            lines[1].push (inR);

            if (tuneOn && detector.push (0.5f * (inL + inR)))
                updateTarget (detector.getFrequency());

            onSmooth += ((tuneOn ? 1.f : 0.f) - onSmooth) * onCoef;
            corr += (corrTarget - corr) * corrCoef;
            shiftSm += (shiftTarget - shiftSm) * shiftCoef;

            const float semis = corr + shiftSm;
            const float ratio = std::exp2 (semis / 12.f);

            // Quand la correction est ~nulle on reste sur le chemin sec : évite tout filtrage en peigne.
            float k = juce::jlimit (0.f, 1.f, std::abs (semis) / 0.12f);
            k = k * k * (3.f - 2.f * k) * onSmooth;

            if (k > 1.0e-4f)
            {
                const float g0 = 0.5f - 0.5f * std::cos ((float) (kTwoPi * u[0]));
                const float g1 = 0.5f - 0.5f * std::cos ((float) (kTwoPi * u[1]));
                const float d0 = (float) u[0] * W[0], d1 = (float) u[1] * W[1];

                for (int ch = 0; ch < 2; ++ch)
                {
                    const float dry = lines[ch].readInt (halfWin);
                    const float wet = g0 * lines[ch].readHermite (d0) + g1 * lines[ch].readHermite (d1);
                    d[ch][i] = dry + (wet - dry) * k;
                }

                // la phase avance de (1 - ratio) / W par échantillon : vitesse de lecture = ratio
                for (int h = 0; h < 2; ++h)
                    u[h] += (double) (1.f - ratio) / (double) W[h];

                // une tête qui sort de sa plage (gain nul) est replacée à l'opposé de l'autre,
                // avec une fenêtre recalée sur un nombre entier de périodes
                for (int h = 0; h < 2; ++h)
                    if (u[h] >= 1.0 || u[h] < 0.0)
                    {
                        W[h] = computeWindow();
                        u[h] = juce::jlimit (0.0, 1.0, u[1 - h] + (ratio > 1.f ? 0.5 : -0.5));
                    }
            }
            else
            {
                // têtes au repos, ré-alignées sur la période courante (inaudibles : k ≈ 0)
                resetHeads (ratio);
                d[0][i] = lines[0].readInt (halfWin);
                d[1][i] = lines[1].readInt (halfWin);
            }
        }
    }

private:
    static constexpr int kScales[6][12] = {
        { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },   // Chromatic
        { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 },   // Major
        { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 },   // Minor
        { 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0 },   // Pentatonic major
        { 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0 },   // Pentatonic minor
        { 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0 },   // Blues
    };

    /** Longueur de fenêtre = 2 x K périodes, K choisi pour rester proche de 25 ms. */
    float computeWindow() const noexcept
    {
        if (periodEst <= 1.f)
            return (float) winLen;
        const float K = std::max (1.f, std::round ((float) halfWin / periodEst));
        return juce::jlimit ((float) halfWin, (float) (2 * winLen - 16), 2.f * K * periodEst);
    }

    void resetHeads (float ratio) noexcept
    {
        W[0] = W[1] = computeWindow();
        u[0] = 0.5;
        u[1] = ratio > 1.f ? 1.0 : 0.0;
    }

    bool inScale (int note) const noexcept { return kScales[scale][((note - keyIdx) % 12 + 12) % 12] != 0; }

    int nearestNote (float midi) const noexcept
    {
        const int base = (int) std::lround (midi);
        int best = base;
        float bestDist = 1.0e9f;
        for (int k = -6; k <= 6; ++k)
        {
            const int n = base + k;
            if (! inScale (n)) continue;
            const float dist = std::abs (midi - (float) n);
            if (dist < bestDist) { bestDist = dist; best = n; }
        }
        return best;
    }

    void updateTarget (float f) noexcept
    {
        if (f <= 0.f)
        {
            if (++unvoicedHops > holdHops)
            {
                corrTarget = 0.f;
                prevNote = -1;
                histCount = 0;
                detMidi.store (-1.f);
                tgtMidi.store (-1.f);
            }
            return;
        }

        unvoicedHops = 0;

        hist[histPos] = f;
        histPos = (histPos + 1) % 3;
        histCount = std::min (histCount + 1, 3);

        float fm = f;
        if (histCount == 3)
            fm = std::max (std::min (hist[0], hist[1]), std::min (std::max (hist[0], hist[1]), hist[2]));

        periodEst = (float) sampleRate / fm;
        const float midi = 69.f + 12.f * std::log2 (fm / 440.f);
        int note = nearestNote (midi);

        // Hystérésis : on ne saute de note que si la nouvelle est nettement plus proche.
        if (prevNote >= 0 && prevNote != note && inScale (prevNote)
            && std::abs (midi - (float) prevNote) < std::abs (midi - (float) note) + 0.18f)
            note = prevNote;

        prevNote = note;
        corrTarget = juce::jlimit (-3.f, 3.f, ((float) note - midi) * amount);
        detMidi.store (midi);
        tgtMidi.store ((float) note);
    }

    double sampleRate = 44100.0;
    int halfWin = 551, winLen = 1102, hopSamples = 288, holdHops = 10;
    DelayLine lines[2];
    PitchDetector detector;

    bool tuneOn = false;
    int keyIdx = 0, scale = 0;
    float amount = 1.f;
    double u[2] = { 0.5, 0.0 };       // phase de chaque tête (0..1) ; retard = u * W
    float W[2] = { 1102.f, 1102.f };
    float periodEst = 0.f;            // période de la voix en échantillons (0 = inconnue)
    float corr = 0.f, corrTarget = 0.f, corrCoef = 1.f;
    float shiftSm = 0.f, shiftTarget = 0.f, shiftCoef = 0.01f;
    float onSmooth = 0.f, onCoef = 0.01f;

    int unvoicedHops = 1000, prevNote = -1, histCount = 0, histPos = 0;
    float hist[3] = { 0.f, 0.f, 0.f };

    std::atomic<float> detMidi { -1.f }, tgtMidi { -1.f };
};

//==============================================================================
//  EQ paramétrique 4 bandes : low shelf / bell / bell / high shelf
//==============================================================================
class ParametricEq
{
public:
    void prepare (double sr) { sampleRate = sr; for (auto& b : bands) b.reset(); }

    void setBand (int i, float freq, float q, float gainDb) noexcept
    {
        configureEqBand (bands[i], i, sampleRate, freq, q, gainDb);
    }

    void process (float* const* d, int n) noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                float x = d[ch][i];
                x = bands[0].process (x, ch);
                x = bands[1].process (x, ch);
                x = bands[2].process (x, ch);
                x = bands[3].process (x, ch);
                d[ch][i] = x;
            }
    }

private:
    double sampleRate = 44100.0;
    Biquad bands[4];
};

//==============================================================================
//  Compresseur feed-forward, détection peak liée stéréo, soft-knee 6 dB,
//  ballistique dans le domaine log, mix parallèle.
//==============================================================================
class Compressor
{
public:
    void prepare (double sr) { sampleRate = sr; grState = 0.f; }

    void setParams (float threshDb, float ratio, float attackMs, float releaseMs, float makeupDb, float mix01) noexcept
    {
        thresholdDb = threshDb;
        slope = 1.f / std::max (1.f, ratio) - 1.f;
        atk = std::exp (-1.f / (0.001f * std::max (0.05f, attackMs)  * (float) sampleRate));
        rel = std::exp (-1.f / (0.001f * std::max (1.0f,  releaseMs) * (float) sampleRate));
        makeup = makeupDb;
        mix = mix01;
    }

    /** Renvoie la réduction de gain max du bloc (dB, valeur positive). */
    float process (float* const* d, int n) noexcept
    {
        constexpr float knee = 6.f;
        float maxGR = 0.f;

        for (int i = 0; i < n; ++i)
        {
            const float x0 = d[0][i], x1 = d[1][i];
            const float lvlDb = linToDb (std::max (std::abs (x0), std::abs (x1)));
            const float over = lvlDb - thresholdDb;

            float gr = 0.f;
            if (2.f * over >= -knee)
            {
                if (2.f * std::abs (over) <= knee)
                {
                    const float t = over + knee * 0.5f;
                    gr = slope * t * t / (2.f * knee);
                }
                else
                    gr = slope * over;
            }

            grState = (gr < grState) ? atk * grState + (1.f - atk) * gr
                                     : rel * grState + (1.f - rel) * gr;

            const float g = dbToLin (grState + makeup);
            const float k = 1.f + (g - 1.f) * mix;
            d[0][i] = x0 * k;
            d[1][i] = x1 * k;
            maxGR = std::max (maxGR, -grState);
        }
        return maxGR;
    }

private:
    double sampleRate = 44100.0;
    float thresholdDb = -18.f, slope = -0.75f, atk = 0.99f, rel = 0.999f, makeup = 0.f, mix = 1.f, grState = 0.f;
};

//==============================================================================
//  De-esser : détection en bande étroite, réduction dynamique de la bande seule
//==============================================================================
class DeEsser
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        bp.reset();
        envAtk = coefFromMs (sr, 0.3f);  envRel = coefFromMs (sr, 15.f);
        redAtk = coefFromMs (sr, 0.5f);  redRel = coefFromMs (sr, 30.f);
        env = 0.f; red = 0.f;
    }

    void setParams (float freq, float threshDb, float amount01) noexcept
    {
        bp.set (Biquad::Type::BandPass, sampleRate, freq, 1.4);
        thresh = threshDb;
        maxRed = amount01 * 18.f;
    }

    void process (float* const* d, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float bl = bp.process (d[0][i], 0);
            const float br = bp.process (d[1][i], 1);
            const float lvl = std::max (std::abs (bl), std::abs (br));
            env += (lvl - env) * (lvl > env ? envAtk : envRel);

            const float over = linToDb (env) - thresh;
            const float target = juce::jlimit (0.f, maxRed, over);
            red += (target - red) * (target > red ? redAtk : redRel);

            const float cut = 1.f - dbToLin (-red);
            d[0][i] -= bl * cut;
            d[1][i] -= br * cut;
        }
    }

private:
    double sampleRate = 44100.0;
    Biquad bp;
    float envAtk = 0.1f, envRel = 0.001f, redAtk = 0.1f, redRel = 0.001f;
    float env = 0.f, red = 0.f, thresh = -30.f, maxRed = 9.f;
};

//==============================================================================
//  Saturation : Tape / Tube / Hard / Fold, tone (high-shelf), mix, DC-block
//==============================================================================
class Saturator
{
public:
    void prepare (double sr) { sampleRate = sr; tone.reset(); dcBlock.reset(); }

    void setParams (int typeIdx, float driveDb, float toneDb, float mix01) noexcept
    {
        type = typeIdx;
        gain = dbToLin (driveDb);
        comp = 1.f / std::sqrt (gain);
        tone.set (Biquad::Type::HighShelf, sampleRate, 2800.0, 0.7, toneDb);
        dcBlock.set (Biquad::Type::HighPass, sampleRate, 20.0, 0.7071);
        mix = mix01;
    }

    void process (float* const* d, int n) noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const float x = d[ch][i];
                float w = shape (x * gain) * comp;
                w = tone.process (w, ch);
                w = dcBlock.process (w, ch);
                d[ch][i] = x + (w - x) * mix;
            }
    }

private:
    inline float shape (float x) const noexcept
    {
        switch (type)
        {
            case 0:  return std::tanh (x);                               // Tape : doux, harmoniques impaires
            case 1:  return std::tanh (x + 0.35f) - 0.33638f;            // Tube : asymétrique, harmoniques paires
            case 2:  { x = juce::jlimit (-1.f, 1.f, x); return 1.5f * x - 0.5f * x * x * x; }   // Hard : quasi-carré
            default: return std::sin (x * 1.5707963f);                   // Fold : repli d'onde, métallique
        }
    }

    double sampleRate = 44100.0;
    int type = 0;
    float gain = 2.f, comp = 0.7f, mix = 1.f;
    Biquad tone, dcBlock;
};

//==============================================================================
//  Doubler : deux voix désaccordées par modulation de retard, décorrélées L/R
//==============================================================================
class Doubler
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& l : lines) l.prepare ((int) (0.06 * sr));
        phase = 0.0;
    }

    void setParams (float rateHz, float depth01, float mix01) noexcept { rate = rateHz; depth = depth01; mix = mix01; }

    void process (float* const* d, int n) noexcept
    {
        static constexpr float centreMs[2] = { 14.f, 19.f };

        for (int i = 0; i < n; ++i)
        {
            lines[0].push (d[0][i]);
            lines[1].push (d[1][i]);

            for (int ch = 0; ch < 2; ++ch)
            {
                const float lfo = std::sin ((float) (kTwoPi * (phase + 0.5 * ch)));
                const float ms = centreMs[ch] + depth * 6.f * lfo;
                const float wet = lines[ch].readHermite (ms * 0.001f * (float) sampleRate);
                d[ch][i] += wet * mix * 0.8f;
            }

            phase += (double) rate / sampleRate;
            phase -= std::floor (phase);
        }
    }

private:
    double sampleRate = 44100.0, phase = 0.0;
    DelayLine lines[2];
    float rate = 0.6f, depth = 0.4f, mix = 0.3f;
};

//==============================================================================
//  Delay stéréo : free / sync tempo, filtre dans le feedback, ping-pong
//==============================================================================
class StereoDelay
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        maxDelay = (float) (2.05 * sr);
        for (auto& l : line) l.prepare ((int) maxDelay + 8);
        fbHp.reset(); fbLp.reset();
        timeSm = targetMs;
    }

    void setParams (float timeMs, float fb01, float toneHz, float mix01, bool pingPong) noexcept
    {
        targetMs = timeMs;
        fb = fb01;
        mix = mix01;
        pp = pingPong;
        fbHp.set (Biquad::Type::HighPass, sampleRate, 140.0, 0.7071);
        fbLp.set (Biquad::Type::LowPass,  sampleRate, toneHz, 0.7071);
    }

    void process (float* const* d, int n) noexcept
    {
        const float tCoef = coefFromMs (sampleRate, 90.f);

        for (int i = 0; i < n; ++i)
        {
            timeSm += (targetMs - timeSm) * tCoef;
            const float ds = juce::jlimit (1.f, maxDelay - 4.f, timeSm * 0.001f * (float) sampleRate) - 1.f;

            const float tapL = line[0].readLinear (ds);
            const float tapR = line[1].readLinear (ds);

            float fbL = fbLp.process (fbHp.process (tapL, 0), 0) * fb;
            float fbR = fbLp.process (fbHp.process (tapR, 1), 1) * fb;

            const float inL = d[0][i], inR = d[1][i];
            float wL, wR;
            if (pp)
            {
                wL = 0.5f * (inL + inR) + fbR;
                wR = fbL;
            }
            else
            {
                wL = inL + fbL;
                wR = inR + fbR;
            }

            line[0].push (juce::jlimit (-8.f, 8.f, wL));
            line[1].push (juce::jlimit (-8.f, 8.f, wR));

            d[0][i] = inL + tapL * mix;
            d[1][i] = inR + tapR * mix;
        }
    }

private:
    double sampleRate = 44100.0;
    float maxDelay = 90000.f;
    DelayLine line[2];
    Biquad fbHp, fbLp;
    float targetMs = 300.f, timeSm = 300.f, fb = 0.3f, mix = 0.15f;
    bool pp = false;
};

//==============================================================================
//  Reverb : pre-delay + low-cut sur l'entrée, juce::Reverb (Freeverb), envoi additif
//==============================================================================
class ReverbFx
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        reverb.setSampleRate (sr);
        reverb.reset();
        pre.prepare ((int) (0.26 * sr));
        hp.reset();
    }

    void setParams (float size01, float damp01, float width01, float preMs, float lowcutHz, float mix01) noexcept
    {
        juce::Reverb::Parameters p;
        p.roomSize = size01;
        p.damping = damp01;
        p.width = width01;
        p.wetLevel = 0.33f;
        p.dryLevel = 0.f;
        p.freezeMode = 0.f;
        reverb.setParameters (p);

        preSamples = juce::jlimit (0, (int) (0.25 * sampleRate), (int) std::lround (preMs * 0.001 * sampleRate));
        hp.set (Biquad::Type::HighPass, sampleRate, lowcutHz, 0.7071);
        mix = mix01;
    }

    void process (float* const* d, int n) noexcept
    {
        float wl[kChunk], wr[kChunk];

        for (int i = 0; i < n; ++i)
        {
            pre.push (0.5f * (d[0][i] + d[1][i]));
            const float x = hp.process (pre.readInt (preSamples), 0);
            wl[i] = wr[i] = x;
        }

        reverb.processStereo (wl, wr, n);

        for (int i = 0; i < n; ++i)
        {
            d[0][i] += wl[i] * mix * kWetScale;
            d[1][i] += wr[i] * mix * kWetScale;
        }
    }

    static constexpr float kWetScale = 1.0f;

private:
    double sampleRate = 44100.0;
    juce::Reverb reverb;
    DelayLine pre;
    Biquad hp;
    int preSamples = 0;
    float mix = 0.15f;
};

//==============================================================================
//  Soft clip de sécurité (-1 dBFS → asymptote à 0 dBFS)
//==============================================================================
inline float softClip (float x) noexcept
{
    constexpr float t = 0.891f;
    const float a = std::abs (x);
    if (a <= t)
        return x;
    const float y = t + (1.f - t) * std::tanh ((a - t) / (1.f - t));
    return x < 0.f ? -y : y;
}

} // namespace bsm

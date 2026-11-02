#pragma once

#include <JuceHeader.h>
#include <array>
#include "DSP.h"
#include "Params.h"
#include "Presets.h"

class BasementAudioProcessor : public juce::AudioProcessor
{
public:
    BasementAudioProcessor();
    ~BasementAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset.load(); }
    void setCurrentProgram (int index) override { applyPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Charge un preset : remet tout à zéro puis applique les valeurs du preset. */
    void applyPreset (int index);

    juce::AudioProcessorValueTreeState apvts;

    // --- données pour l'interface (lues par le thread GUI) -------------------
    std::atomic<float> meterIn { 0.f }, meterOut { 0.f }, meterGR { 0.f };
    float getDetectedMidi() const { return tune.getDetectedMidi(); }
    float getTargetMidi() const   { return tune.getTargetMidi(); }
    int getLatencyInSamples() const { return latency; }

private:
    template <typename F>
    void runToggle (juce::SmoothedValue<float>& s, float* const* d, int n, F&& fn);

    float P (int id) const noexcept { return params[(size_t) id]->load(); }

    std::array<std::atomic<float>*, PID::Count> params {};

    double sr = 44100.0;
    int latency = 0;

    bsm::HighPass24 hpf;
    bsm::Gate gate;
    bsm::AutoTune tune;
    bsm::ParametricEq eq;
    bsm::Compressor comp;
    bsm::DeEsser deess;
    bsm::Saturator sat;
    bsm::Doubler dbl;
    bsm::StereoDelay dly;
    bsm::ReverbFx rev;
    bsm::DelayLine dryLine[2];

    juce::SmoothedValue<float> inGainSm, outGainSm, mixSm;
    juce::SmoothedValue<float> tgEq, tgComp, tgDeess, tgSat, tgDbl, tgDly, tgRev;

    std::atomic<int> currentPreset { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasementAudioProcessor)
};

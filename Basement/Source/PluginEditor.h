#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/Components.h"

//==============================================================================
//  Interface complète, dessinée sur une grille fixe de 1120 x 584 puis mise à l'échelle
//  (la fenêtre est redimensionnable en gardant ses proportions).
//==============================================================================
class BasementUI : public juce::Component, private juce::Timer
{
public:
    static constexpr int kWidth = 1120, kHeight = 584;

    explicit BasementUI (BasementAudioProcessor&);
    ~BasementUI() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void selectPreset (int index);
    void showPresetMenu();
    static juce::Image makeGrain();

    BasementAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    BsmLookAndFeel lnf;

    ui::EqCurve eqCurve;
    ui::GrMeter grMeter;
    ui::PitchReadout pitch;

    ui::Panel inputP, tuneP, eqP, compP, deessP, satP, dblP, dlyP, revP, outP;
    ui::KnobComponent* delayTimeKnob = nullptr;

    ui::ChevronButton prevBtn { false }, nextBtn { true };

    juce::Image grain;
    juce::Rectangle<int> headerArea, nameArea, footerArea;

    int shownPreset = -1;
    juce::Colour currentAccent { 0xffff5fb0 }, targetAccent { 0xffff5fb0 };
    float inDisp = 0.f, outDisp = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasementUI)
};

//==============================================================================
class BasementAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit BasementAudioProcessorEditor (BasementAudioProcessor&);
    ~BasementAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    BasementUI ui;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasementAudioProcessorEditor)
};

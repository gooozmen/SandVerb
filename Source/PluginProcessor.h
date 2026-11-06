#pragma once
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "SandVerbCore.h"

class SandVerbProcessor : public juce::AudioProcessor
{
public:
    SandVerbProcessor();
    ~SandVerbProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SandVerb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 40.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Ядро открыто для редактора: он читает из него значения для индикаторов
    SandVerbCore core;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    int computeTargetDelaySamples() const;

    // Кэш указателей на параметры: в аудиопотоке не ищем их по строке
    enum ParamIndex { P_frame, P_sync, P_division, P_feel, P_frames, P_falloff, P_fbon,
                      P_send, P_sendon, P_retout,
                      P_verbon, P_verbtype, P_rt60, P_damping, P_mix,
                      P_comp1on, P_c1thr, P_c1ratio, P_comp2on, P_c2thr, P_c2ratio,
                      P_sandon, P_sandlevel,
                      P_driveon, P_drvtype, P_drive, P_tone, P_drvlevel,
                      P_crushon, P_bits, P_rate, P_compand, P_cnoise, P_lowshelf, P_highshelf, P_cmix,
                      P_lfo1rate, P_lfo1amt, P_lfo2rate, P_lfo2amt, P_rndrate, P_rndamt, P_envamt, kNumParams };
    static const char* const kParamIds[kNumParams];
    std::atomic<float>* param[kNumParams] = {};
    float get(ParamIndex i) const { return param[i]->load(); }

    double sr = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SandVerbProcessor)
};

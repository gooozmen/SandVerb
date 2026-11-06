#pragma once
#include "PluginProcessor.h"
#include "Widgets.h"
#include "FlowGraph.h"
#include "Inspector.h"

class SandVerbEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SandVerbEditor(SandVerbProcessor&);
    ~SandVerbEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    SandLookAndFeel laf;                    // первым: переживает компоненты
    SandVerbProcessor& proc;

    FlowGraph graph;                        // схема: выбор блока и включатели
    Inspector inspector;                    // настройки выбранного блока
    juce::Label readout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SandVerbEditor)
};

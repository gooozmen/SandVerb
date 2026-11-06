#pragma once
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui {
    inline const juce::Colour bgTop   (0xff1f1a13);
    inline const juce::Colour bgBottom(0xff14110c);
    inline const juce::Colour panel   (0xff2a241b);
    inline const juce::Colour line    (0xff3d3527);
    inline const juce::Colour sand    (0xffe2b872);
    inline const juce::Colour sandHi  (0xfff5e2b8);
    inline const juce::Colour text    (0xffd9c9a3);
    inline const juce::Colour textDim (0xff9a8d6f);
    inline const juce::Colour loop    (0xffd9824b);   // цвет петли обратной связи
}

// Блоки графа. По нажатию на блок в инспекторе открываются его настройки.
enum class Node { In, Reverb, Comp1, Feedback, Mod, Sand, Comp2, Send, Out, kCount };

// ID включателя блока в APVTS (nullptr — у блока нет включателя)
inline const char* switchIdOf(Node n)
{
    static const char* const ids[(int)Node::kCount] = {
        nullptr, "verbon", "comp1on", "fbon", nullptr, "sandon", "comp2on", "sendon", nullptr };   // порядок как в Node
    return (int)n >= 0 && (int)n < (int)Node::kCount ? ids[(int)n] : nullptr;
}

// Песочная тема для стандартных компонентов JUCE
class SandLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SandLookAndFeel();
};

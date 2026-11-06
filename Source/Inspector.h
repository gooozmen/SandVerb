#pragma once
#include <array>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "Widgets.h"

// Настройки выбранного блока. По одной странице на блок: страница делится на секции
// (например, SAND = DRIVE + CRUSHER + SAND), у каждой секции свой включатель.
class Inspector : public juce::Component
{
public:
    explicit Inspector(SandVerbProcessor&);

    void select(Node);
    Node selected() const { return current; }
    void refresh();                      // по таймеру: доступность ручек, индикатор подавления

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct Knob {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label  label;
        std::unique_ptr<APVTS::SliderAttachment> attach;      // объявлен после slider => умирает раньше
    };
    struct Choice {
        juce::ComboBox box;
        juce::Label    caption;
        std::unique_ptr<APVTS::ComboBoxAttachment> attach;
    };
    struct Switch {
        juce::ToggleButton button;
        juce::String paramId;
        std::unique_ptr<APVTS::ButtonAttachment> attach;
    };
    struct Section {
        juce::String title;
        Switch* enable = nullptr;                // включатель в заголовке секции
        std::vector<Switch*> switches;           // дополнительные кнопки в теле секции
        std::vector<Choice*> choices;
        std::vector<Knob*>   knobs;
        float weight = 1.f;                      // доля ширины страницы
        juce::Rectangle<int> bounds;
    };
    struct Page { std::vector<Section> sections; bool hasMeter = false; int meterIndex = 0; };

    Knob*   knob(const char* paramId, const juce::String& text);
    Choice* choice(const char* paramId, const juce::String& text);
    Switch* sw(const char* paramId, const juce::String& text);
    Section& section(Node page, const juce::String& title, Switch* enable, float weight);

    static Node pageOf(Node n) { return n == Node::Out ? Node::In : n; }
    void setVisibleAll(Section&, bool);
    static void layoutKnobRow(juce::Rectangle<int> area, const std::vector<Knob*>& knobs);
    static void layoutChoice(juce::Rectangle<int> area, Choice& c);
    float raw(const juce::String& id) const { return proc.apvts.getRawParameterValue(id)->load(); }

    SandVerbProcessor& proc;
    std::vector<std::unique_ptr<Knob>>   knobStore;
    std::vector<std::unique_ptr<Choice>> choiceStore;
    std::vector<std::unique_ptr<Switch>> switchStore;
    std::array<Page, (size_t)Node::kCount> pages;
    Node current = Node::Sand;
    Knob* frameKnob = nullptr;
    Choice *divisionBox = nullptr, *feelBox = nullptr;
    juce::Rectangle<int> meterRect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Inspector)
};

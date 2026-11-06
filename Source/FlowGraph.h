#pragma once
#include <functional>
#include <vector>
#include "PluginProcessor.h"
#include "Widgets.h"

// Интерактивная схема сигнала. Нажатие на блок выбирает его (открывает настройки),
// нажатие на светодиод в углу блока включает или выключает блок.
class FlowGraph : public juce::Component
{
public:
    explicit FlowGraph(SandVerbProcessor&);

    std::function<void(Node)> onSelect;

    void setSelected(Node n) { selected = n; repaint(); }
    Node getSelected() const { return selected; }

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    struct NodeDef { Node id; const char* name; float cx, cy, w, h; };
    struct Wire {
        std::vector<juce::Point<float>> pts;     // нормированные координаты 0..1
        Node a, b;                               // блоки на концах: выключен любой — провод тускнеет
        int  lvl;                                // индекс в SandVerbCore::uiLvl (-1 — без индикации)
        bool arrow = true, dashed = false, loopColour = false;
    };

    juce::Rectangle<float> inner() const;
    juce::Point<float> pt(juce::Point<float> n) const;
    juce::Rectangle<float> boxOf(const NodeDef&) const;
    bool isOn(Node) const;
    juce::String subText(Node) const;
    const NodeDef* hit(juce::Point<float> p, bool* onLed) const;
    float raw(const char* id) const { return proc.apvts.getRawParameterValue(id)->load(); }

    SandVerbProcessor& proc;
    std::vector<NodeDef> nodes;
    std::vector<Wire> wires;
    Node selected = Node::Sand;
    Node hover = Node::kCount;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlowGraph)
};

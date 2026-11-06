#include "FlowGraph.h"

namespace {
    constexpr float kLedR = 4.5f;

    juce::Point<float> ledCentre(const juce::Rectangle<float>& b) { return { b.getX() + 11.f, b.getY() + 11.f }; }

    juce::String rateText(float hz)
    {
        return hz >= 1000.f ? juce::String(hz / 1000.f, 1) + "k" : juce::String(juce::roundToInt(hz));
    }
}

FlowGraph::FlowGraph(SandVerbProcessor& p) : proc(p)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    using C = SandVerbCore;
    nodes = {
        { Node::In,       "IN",       0.045f, 0.46f, 0.062f, 0.16f },
        { Node::Reverb,   "REVERB",   0.190f, 0.46f, 0.115f, 0.17f },
        { Node::Comp1,    "COMP 1",   0.345f, 0.46f, 0.105f, 0.17f },
        { Node::Sand,     "SAND",     0.520f, 0.28f, 0.130f, 0.17f },
        { Node::Feedback, "FEEDBACK", 0.520f, 0.66f, 0.130f, 0.17f },
        { Node::Comp2,    "COMP 2",   0.700f, 0.46f, 0.105f, 0.17f },
        { Node::Out,      "OUT",      0.955f, 0.46f, 0.062f, 0.16f },
        { Node::Send,     "SEND",     0.520f, 0.91f, 0.100f, 0.15f },
        { Node::Mod,      "MOD",      0.340f, 0.82f, 0.100f, 0.15f } };

    auto W = [&](std::vector<juce::Point<float>> pts, Node a, Node b, int lvl, bool arrow = true) {
        Wire w; w.pts = std::move(pts); w.a = a; w.b = b; w.lvl = lvl; w.arrow = arrow; return w; };

    wires.push_back(W({ {0.045f, 0.38f}, {0.045f, 0.07f}, {0.955f, 0.07f}, {0.955f, 0.38f} }, Node::In, Node::Out, C::L_In));                      // dry
    wires.push_back(W({ {0.076f, 0.46f}, {0.1325f, 0.46f} }, Node::In, Node::Reverb, C::L_In));
    wires.push_back(W({ {0.2475f, 0.46f}, {0.2925f, 0.46f} }, Node::Reverb, Node::Comp1, C::L_Verb));
    wires.push_back(W({ {0.285f, 0.46f}, {0.285f, 0.15f}, {0.89f, 0.15f}, {0.89f, 0.46f} }, Node::Reverb, Node::Out, C::L_Verb, false));         // reverb -> Mix
    wires.push_back(W({ {0.3975f, 0.46f}, {0.43f, 0.46f}, {0.43f, 0.28f}, {0.455f, 0.28f} }, Node::Comp1, Node::Sand, C::L_Comp1));
    wires.push_back(W({ {0.43f, 0.46f}, {0.43f, 0.66f}, {0.455f, 0.66f} }, Node::Comp1, Node::Feedback, C::L_Comp1));
    wires.push_back(W({ {0.585f, 0.28f}, {0.625f, 0.28f}, {0.625f, 0.46f}, {0.6475f, 0.46f} }, Node::Sand, Node::Comp2, C::L_Sand));
    wires.push_back(W({ {0.585f, 0.66f}, {0.625f, 0.66f}, {0.625f, 0.46f} }, Node::Feedback, Node::Comp2, C::L_Fb, false));
    wires.push_back(W({ {0.7525f, 0.46f}, {0.89f, 0.46f}, {0.924f, 0.46f} }, Node::Comp2, Node::Out, C::L_Comp2));                                  // Fb Out
    {   Wire w = W({ {0.70f, 0.5425f}, {0.70f, 0.91f}, {0.57f, 0.91f} }, Node::Comp2, Node::Send, C::L_Comp2); w.loopColour = true; wires.push_back(w); }
    {   Wire w = W({ {0.52f, 0.835f}, {0.52f, 0.745f} }, Node::Send, Node::Feedback, C::L_Send); w.loopColour = true; wires.push_back(w); }
    {   Wire w = W({ {0.39f, 0.82f}, {0.47f, 0.745f} }, Node::Mod, Node::Feedback, -1); w.dashed = true; w.loopColour = true; wires.push_back(w); }
}

juce::Rectangle<float> FlowGraph::inner() const { return getLocalBounds().toFloat().reduced(10.f, 8.f).withTrimmedTop(12.f); }

juce::Point<float> FlowGraph::pt(juce::Point<float> n) const
{
    const auto r = inner();
    return { r.getX() + n.x * r.getWidth(), r.getY() + n.y * r.getHeight() };
}

juce::Rectangle<float> FlowGraph::boxOf(const NodeDef& d) const
{
    const auto r = inner();
    return juce::Rectangle<float>(d.w * r.getWidth(), d.h * r.getHeight()).withCentre(pt({ d.cx, d.cy }));
}

bool FlowGraph::isOn(Node n) const
{
    const char* id = switchIdOf(n);
    return id == nullptr || raw(id) > 0.5f;
}

juce::String FlowGraph::subText(Node n) const
{
    if (n == Node::Reverb)   return juce::String(raw("verbtype") > 0.5f ? "Tape" : "Fat") + " / " + juce::String(raw("rt60"), 1) + " s";
    if (n == Node::Comp1)    return "GR " + juce::String(proc.core.uiGr1.load(), 1) + " dB";
    if (n == Node::Comp2)    return "GR " + juce::String(proc.core.uiGr2.load(), 1) + " dB";
    if (n == Node::Feedback) return juce::String(proc.core.uiFrames.load(), 1) + " fr / " + juce::String(juce::roundToInt(proc.core.uiDelayMs.load())) + " ms";
    if (n == Node::Send)     return juce::String(juce::roundToInt(raw("send") * 100.f)) + " %";
    if (n == Node::Mod)      return "-> depth";
    if (n == Node::Sand) {
        static const char* names[] = { "OD", "Tube", "Fuzz" };
        juce::StringArray parts;
        if (raw("driveon") > 0.5f) parts.add(names[juce::jlimit(0, 2, juce::roundToInt(raw("drvtype")))]);
        if (raw("crushon") > 0.5f) parts.add(juce::String(raw("bits"), 1) + "b " + rateText(raw("rate")));
        return parts.isEmpty() ? juce::String("bypass") : parts.joinIntoString(" / ");
    }
    return {};
}

void FlowGraph::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setColour(ui::panel);
    g.fillRoundedRectangle(area, 10.f);
    g.setColour(ui::line);
    g.drawRoundedRectangle(area.reduced(0.5f), 10.f, 1.f);

    g.setColour(ui::textDim);
    g.setFont(juce::Font(juce::FontOptions(10.5f)));
    g.drawText("SIGNAL FLOW   -   click a block to edit it, click its LED to switch it on/off",
               juce::Rectangle<int>(14, 4, getWidth() - 28, 14), juce::Justification::centredLeft);

    // провода
    for (const auto& w : wires) {
        const bool alive = isOn(w.a) && isOn(w.b);
        const float lv = w.lvl >= 0 ? proc.core.uiLvl[w.lvl].load() : 0.5f;
        const auto base = w.loopColour ? ui::loop : ui::textDim;
        const auto hi   = w.loopColour ? ui::loop.brighter(0.5f) : ui::sandHi;
        g.setColour(base.interpolatedWith(hi, lv).withAlpha(alive ? 1.f : 0.25f));
        const float th = 1.4f + 1.6f * lv;

        for (size_t i = 0; i + 1 < w.pts.size(); ++i) {
            const auto a = pt(w.pts[i]), b = pt(w.pts[i + 1]);
            const bool last = (i + 2 == w.pts.size());
            if (last && w.arrow)
                g.drawArrow(juce::Line<float>(a, b), th, 7.f + th, 7.f + th);
            else if (w.dashed) {
                const float dash[] = { 4.f, 3.f };
                g.drawDashedLine(juce::Line<float>(a, b), dash, 2, th);
            } else
                g.drawLine(juce::Line<float>(a, b), th);
        }
    }

    // подписи на проводах
    g.setColour(ui::textDim);
    g.setFont(juce::Font(juce::FontOptions(9.5f)));
    auto label = [&](const char* t, juce::Point<float> p, float dy) {
        const auto c = pt(p);
        g.drawText(t, juce::Rectangle<float>(60.f, 12.f).withCentre({ c.x, c.y + dy }).toNearestInt(), juce::Justification::centred);
    };
    label("dry",    { 0.50f, 0.07f }, -8.f);
    label("mix",    { 0.78f, 0.15f }, -8.f);
    label("Fb Out", { 0.82f, 0.46f }, 10.f);

    // блоки
    for (const auto& d : nodes) {
        const auto b = boxOf(d);
        const bool on = isOn(d.id), sel = (d.id == selected), hov = (d.id == hover);
        const bool accent = (d.id == Node::Feedback || d.id == Node::Send || d.id == Node::Mod);
        const auto edge = accent ? ui::loop : ui::sand;

        const float fa = on ? 1.f : 0.45f;
        g.setColour(accent ? ui::loop.withAlpha(fa * 0.22f).brighter(hov ? 0.3f : 0.f)
                           : ui::line.brighter(hov ? 0.15f : 0.f).withAlpha(fa));
        g.fillRoundedRectangle(b, 5.f);
        g.setColour(sel ? ui::sandHi : edge.withAlpha(on ? 1.f : 0.4f));
        g.drawRoundedRectangle(b.reduced(0.5f), 5.f, sel ? 2.4f : 1.f);

        const float a = on ? 1.f : 0.45f;
        const auto sub = subText(d.id);
        auto textArea = b.reduced(4.f, 2.f);
        if (switchIdOf(d.id) != nullptr) textArea.removeFromLeft(10.f);

        g.setColour(ui::text.withAlpha(a));
        g.setFont(juce::Font(juce::FontOptions(12.f, juce::Font::bold)));
        if (sub.isEmpty()) {
            g.drawText(d.name, textArea.toNearestInt(), juce::Justification::centred);
        } else {
            g.drawText(d.name, textArea.removeFromTop(textArea.getHeight() * 0.5f + 2.f).toNearestInt(), juce::Justification::centredBottom);
            g.setColour(ui::textDim.withAlpha(a));
            g.setFont(juce::Font(juce::FontOptions(9.5f)));
            g.drawFittedText(sub, textArea.toNearestInt(), juce::Justification::centredTop, 1);
        }

        if (switchIdOf(d.id) != nullptr) {                       // светодиод включателя
            const auto c = ledCentre(b);
            if (on) {
                g.setColour(ui::sand.withAlpha(0.25f));
                g.fillEllipse(c.x - kLedR - 2.5f, c.y - kLedR - 2.5f, 2.f * (kLedR + 2.5f), 2.f * (kLedR + 2.5f));
                g.setColour(ui::sandHi);
                g.fillEllipse(c.x - kLedR, c.y - kLedR, 2.f * kLedR, 2.f * kLedR);
            } else {
                g.setColour(ui::textDim);
                g.drawEllipse(c.x - kLedR, c.y - kLedR, 2.f * kLedR, 2.f * kLedR, 1.2f);
            }
        }
    }
}

const FlowGraph::NodeDef* FlowGraph::hit(juce::Point<float> p, bool* onLed) const
{
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        const auto b = boxOf(*it);
        if (!b.contains(p)) continue;
        if (onLed) *onLed = switchIdOf(it->id) != nullptr && ledCentre(b).getDistanceFrom(p) <= 10.f;
        return &*it;
    }
    return nullptr;
}

void FlowGraph::mouseDown(const juce::MouseEvent& e)
{
    bool led = false;
    const auto* d = hit(e.position, &led);
    if (d == nullptr) return;

    if (led) {
        if (auto* prm = proc.apvts.getParameter(switchIdOf(d->id))) {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost(prm->getValue() > 0.5f ? 0.f : 1.f);
            prm->endChangeGesture();
        }
    } else {
        selected = d->id;
        if (onSelect) onSelect(d->id);
    }
    repaint();
}

void FlowGraph::mouseMove(const juce::MouseEvent& e)
{
    const auto* d = hit(e.position, nullptr);
    const Node h = d ? d->id : Node::kCount;
    if (h != hover) { hover = h; repaint(); }
}

void FlowGraph::mouseExit(const juce::MouseEvent&)
{
    if (hover != Node::kCount) { hover = Node::kCount; repaint(); }
}

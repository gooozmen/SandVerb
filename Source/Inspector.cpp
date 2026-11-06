#include "Inspector.h"

Inspector::Knob* Inspector::knob(const char* paramId, const juce::String& text)
{
    knobStore.push_back(std::make_unique<Knob>());
    Knob& k = *knobStore.back();
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 18);
    k.label.setText(text, juce::dontSendNotification);
    k.label.setJustificationType(juce::Justification::centred);
    k.label.setFont(juce::Font(juce::FontOptions(11.0f)));
    addChildComponent(k.slider);
    addChildComponent(k.label);
    k.attach = std::make_unique<APVTS::SliderAttachment>(proc.apvts, paramId, k.slider);
    return &k;
}

Inspector::Choice* Inspector::choice(const char* paramId, const juce::String& text)
{
    choiceStore.push_back(std::make_unique<Choice>());
    Choice& c = *choiceStore.back();
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(paramId)))
        c.box.addItemList(p->choices, 1);                 // заполняем до создания attachment
    c.caption.setText(text, juce::dontSendNotification);
    c.caption.setFont(juce::Font(juce::FontOptions(11.0f)));
    addChildComponent(c.box);
    addChildComponent(c.caption);
    c.attach = std::make_unique<APVTS::ComboBoxAttachment>(proc.apvts, paramId, c.box);
    return &c;
}

Inspector::Switch* Inspector::sw(const char* paramId, const juce::String& text)
{
    switchStore.push_back(std::make_unique<Switch>());
    Switch& s = *switchStore.back();
    s.paramId = paramId;
    s.button.setButtonText(text);
    addChildComponent(s.button);
    s.attach = std::make_unique<APVTS::ButtonAttachment>(proc.apvts, paramId, s.button);
    return &s;
}

Inspector::Section& Inspector::section(Node page, const juce::String& title, Switch* enable, float weight)
{
    auto& pg = pages[(size_t)page];
    pg.sections.emplace_back();
    auto& s = pg.sections.back();
    s.title = title; s.enable = enable; s.weight = weight;
    return s;
}

Inspector::Inspector(SandVerbProcessor& p) : proc(p)
{
    {   auto& s = section(Node::Reverb, "REVERB", sw("verbon", "ON"), 1.f);
        s.choices = { choice("verbtype", "TYPE") };
        s.knobs   = { knob("rt60", "DECAY"), knob("damping", "DAMPING"), knob("mix", "MIX") }; }

    {   auto& s = section(Node::Comp1, "COMP 1", sw("comp1on", "ON"), 1.f);
        s.knobs = { knob("c1thr", "THRESHOLD"), knob("c1ratio", "RATIO") };
        pages[(size_t)Node::Comp1].hasMeter = true; }

    {   auto& s = section(Node::Comp2, "COMP 2", sw("comp2on", "ON"), 1.f);
        s.knobs = { knob("c2thr", "THRESHOLD"), knob("c2ratio", "RATIO") };
        pages[(size_t)Node::Comp2].hasMeter = true;
        pages[(size_t)Node::Comp2].meterIndex = 1; }

    {   auto& s = section(Node::Feedback, "FEEDBACK", sw("fbon", "ON"), 1.f);
        s.switches = { sw("sync", "TEMPO SYNC") };
        s.choices  = { choice("division", "DIVISION"), choice("feel", "FEEL") };
        s.knobs    = { knob("frame", "FRAME"), knob("frames", "DEPTH"), knob("falloff", "FALLOFF") };
        frameKnob = s.knobs[0]; divisionBox = s.choices[0]; feelBox = s.choices[1]; }

    {   auto& s = section(Node::Mod, "MODULATION  ->  Depth", nullptr, 1.f);
        s.knobs = { knob("lfo1rate", "LFO1 RATE"), knob("lfo1amt", "LFO1 AMT"), knob("lfo2rate", "LFO2 RATE"), knob("lfo2amt", "LFO2 AMT"),
                    knob("rndrate", "RANDOM RATE"), knob("rndamt", "RANDOM AMT"), knob("envamt", "ENV AMT") }; }

    {   auto& d = section(Node::Sand, "DRIVE", sw("driveon", "ON"), 3.2f);
        d.choices = { choice("drvtype", "TYPE") };
        d.knobs   = { knob("drive", "DRIVE"), knob("tone", "TONE"), knob("drvlevel", "LEVEL") };

        auto& c = section(Node::Sand, "CRUSHER", sw("crushon", "ON"), 7.f);
        c.knobs = { knob("bits", "BITS"), knob("rate", "RATE"), knob("compand", "COMPAND"), knob("cnoise", "NOISE"),
                    knob("lowshelf", "LOW"), knob("highshelf", "HIGH"), knob("cmix", "MIX") };

        auto& s = section(Node::Sand, "SAND", sw("sandon", "ON"), 1.7f);
        s.knobs = { knob("sandlevel", "LEVEL") }; }

    {   auto& s = section(Node::Send, "SEND", sw("sendon", "ON"), 1.f);
        s.knobs = { knob("send", "SEND") }; }

    {   auto& s = section(Node::In, "IN / OUT", nullptr, 1.f);
        s.knobs = { knob("mix", "REVERB MIX"), knob("retout", "FB OUT") }; }

    select(Node::Sand);
}

void Inspector::setVisibleAll(Section& s, bool v)
{
    if (s.enable) s.enable->button.setVisible(v);
    for (auto* x : s.switches) x->button.setVisible(v);
    for (auto* x : s.choices) { x->box.setVisible(v); x->caption.setVisible(v); }
    for (auto* x : s.knobs)   { x->slider.setVisible(v); x->label.setVisible(v); }
}

void Inspector::select(Node n)
{
    current = n;
    for (auto& pg : pages) for (auto& s : pg.sections) setVisibleAll(s, false);
    for (auto& s : pages[(size_t)pageOf(n)].sections) setVisibleAll(s, true);
    resized();
    refresh();
    repaint();
}

void Inspector::layoutKnobRow(juce::Rectangle<int> area, const std::vector<Knob*>& knobs)
{
    if (knobs.empty()) return;
    const int w = area.getWidth() / (int)knobs.size();
    for (auto* k : knobs) {
        auto cell = area.removeFromLeft(w);
        k->label.setBounds(cell.removeFromTop(16));
        k->slider.setBounds(cell);
    }
}

void Inspector::layoutChoice(juce::Rectangle<int> area, Choice& c)
{
    c.caption.setBounds(area.removeFromTop(14));
    c.box.setBounds(area.removeFromTop(26));
}

void Inspector::resized()
{
    auto& pg = pages[(size_t)pageOf(current)];
    if (pg.sections.empty()) return;

    const auto area = getLocalBounds();
    const int gap = 10;
    float total = 0.f;
    for (auto& s : pg.sections) total += s.weight;
    const int avail = area.getWidth() - gap * ((int)pg.sections.size() - 1);

    int x = area.getX();
    meterRect = {};
    for (size_t i = 0; i < pg.sections.size(); ++i) {
        auto& s = pg.sections[i];
        const int share = (int)(avail * s.weight / total);
        const int cap   = std::max(1, (int)s.knobs.size()) * 140 + 24;
        s.bounds = { x, area.getY(), std::min(share, cap), area.getHeight() };
        x += s.bounds.getWidth() + gap;

        auto a = s.bounds.reduced(12);
        auto hdr = a.removeFromTop(26);
        if (s.enable) s.enable->button.setBounds(hdr.removeFromRight(60).withSizeKeepingCentre(60, 24));
        a.removeFromTop(6);

        if (!s.switches.empty() || !s.choices.empty()) {
            auto row = a.removeFromTop(46);
            for (auto* b : s.switches) { b->button.setBounds(row.removeFromLeft(130).withSizeKeepingCentre(130, 26)); row.removeFromLeft(6); }
            int left = (int)s.choices.size();
            for (auto* c : s.choices) { layoutChoice(row.removeFromLeft(row.getWidth() / left).reduced(2, 0), *c); --left; }
            a.removeFromTop(8);
        }
        if (pg.hasMeter && i == 0) { meterRect = a.removeFromBottom(34); a.removeFromBottom(8); }
        layoutKnobRow(a, s.knobs);
    }
}

void Inspector::refresh()
{
    // FEEDBACK: Frame доступна без Sync, Division и Feel — с Sync
    const bool sync = raw("sync") > 0.5f;
    frameKnob->slider.setEnabled(!sync);
    divisionBox->box.setEnabled(sync);
    feelBox->box.setEnabled(sync);

    // у выключенной секции ручки блёклые
    for (auto& s : pages[(size_t)pageOf(current)].sections) {
        const float a = (s.enable == nullptr || raw(s.enable->paramId) > 0.5f) ? 1.f : 0.4f;
        for (auto* k : s.knobs)   { k->slider.setAlpha(a); k->label.setAlpha(a); }
        for (auto* c : s.choices) { c->box.setAlpha(a); c->caption.setAlpha(a); }
    }
    if (!meterRect.isEmpty()) repaint(meterRect);
}

void Inspector::paint(juce::Graphics& g)
{
    auto& pg = pages[(size_t)pageOf(current)];
    for (auto& s : pg.sections) {
        const auto r = s.bounds;
        g.setColour(ui::panel);
        g.fillRoundedRectangle(r.toFloat(), 10.f);
        g.setColour(ui::line);
        g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 10.f, 1.f);
        g.setColour(ui::sand);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText(s.title, r.getX() + 14, r.getY() + 8, r.getWidth() - 90, 18, juce::Justification::centredLeft);
    }

    if (!meterRect.isEmpty()) {                       // индикатор подавления компрессора
        const float gr = pg.meterIndex == 0 ? proc.core.uiGr1.load() : proc.core.uiGr2.load();
        auto m = meterRect.toFloat();
        g.setColour(ui::textDim);
        g.setFont(juce::Font(juce::FontOptions(10.5f)));
        g.drawText("GAIN REDUCTION", m.removeFromTop(14.f).toNearestInt(), juce::Justification::centredLeft);
        g.setColour(ui::line);
        g.fillRoundedRectangle(m.removeFromTop(10.f), 3.f);
        auto bar = meterRect.toFloat().withTop(meterRect.getY() + 15.f).withHeight(10.f);
        g.setColour(ui::loop);
        g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * std::clamp(-gr / 24.f, 0.f, 1.f)), 3.f);
        g.setColour(ui::text);
        g.drawText(juce::String(gr, 1) + " dB", meterRect.withTrimmedTop(26), juce::Justification::centredRight);
    }
}

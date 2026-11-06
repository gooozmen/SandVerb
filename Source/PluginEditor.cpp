#include "PluginEditor.h"

SandLookAndFeel::SandLookAndFeel()
{
    using namespace juce;
    setColour(Slider::rotarySliderFillColourId,    ui::sand);
    setColour(Slider::rotarySliderOutlineColourId, ui::line);
    setColour(Slider::thumbColourId,               ui::sandHi);
    setColour(Slider::textBoxTextColourId,         ui::text);
    setColour(Slider::textBoxOutlineColourId,      Colours::transparentBlack);
    setColour(Slider::textBoxBackgroundColourId,   Colours::transparentBlack);
    setColour(Label::textColourId,                 ui::textDim);

    setColour(ComboBox::backgroundColourId, ui::line);
    setColour(ComboBox::textColourId,       ui::text);
    setColour(ComboBox::outlineColourId,    Colour(0xff55492f));
    setColour(ComboBox::arrowColourId,      ui::sand);
    setColour(PopupMenu::backgroundColourId,            ui::panel);
    setColour(PopupMenu::textColourId,                  ui::text);
    setColour(PopupMenu::highlightedBackgroundColourId, ui::sand);
    setColour(PopupMenu::highlightedTextColourId,       ui::bgTop);

    setColour(ToggleButton::textColourId,         ui::text);
    setColour(ToggleButton::tickColourId,         ui::sand);
    setColour(ToggleButton::tickDisabledColourId, ui::textDim);
}

SandVerbEditor::SandVerbEditor(SandVerbProcessor& p)
    : juce::AudioProcessorEditor(&p), proc(p), graph(p), inspector(p)
{
    setLookAndFeel(&laf);

    addAndMakeVisible(graph);
    addAndMakeVisible(inspector);

    readout.setJustificationType(juce::Justification::centredRight);
    readout.setColour(juce::Label::textColourId, ui::sand);
    addAndMakeVisible(readout);

    // выбранный блок хранится в состоянии проекта
    const int saved = juce::jlimit(0, (int)Node::kCount - 1, (int)proc.apvts.state.getProperty("uiNode", (int)Node::Sand));
    graph.setSelected((Node)saved);
    inspector.select((Node)saved);
    graph.onSelect = [this](Node n) {
        inspector.select(n);
        proc.apvts.state.setProperty("uiNode", (int)n, nullptr);
    };

    setSize(1100, 720);
    startTimerHz(30);
    timerCallback();
}

SandVerbEditor::~SandVerbEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SandVerbEditor::resized()
{
    auto bounds = getLocalBounds().reduced(14);
    auto header = bounds.removeFromTop(40);
    header.removeFromLeft(210);
    readout.setBounds(header);

    bounds.removeFromTop(6);
    graph.setBounds(bounds.removeFromTop(330));
    bounds.removeFromTop(10);
    inspector.setBounds(bounds);
}

void SandVerbEditor::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(ui::bgTop, 0.f, 0.f, ui::bgBottom, 0.f, (float)getHeight(), false));
    g.fillAll();

    g.setColour(ui::sand);
    g.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
    g.drawText("SANDVERB", 18, 12, 300, 34, juce::Justification::centredLeft);
}

void SandVerbEditor::timerCallback()
{
    readout.setText("frame " + juce::String(proc.core.uiDelayMs.load(), 1) + " ms   |   depth "
                    + juce::String(proc.core.uiFrames.load(), 2) + " frames",
                    juce::dontSendNotification);
    graph.repaint();
    inspector.refresh();
}

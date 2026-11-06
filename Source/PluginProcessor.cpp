#include "PluginProcessor.h"
#include "PluginEditor.h"

SandVerbProcessor::SandVerbProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
        param[i] = apvts.getRawParameterValue(kParamIds[i]);
}

const char* const SandVerbProcessor::kParamIds[SandVerbProcessor::kNumParams] = {
    "frame", "sync", "division", "feel", "frames", "falloff", "fbon",
    "send", "sendon", "retout",
    "verbon", "verbtype", "rt60", "damping", "mix",
    "comp1on", "c1thr", "c1ratio", "comp2on", "c2thr", "c2ratio",
    "sandon", "sandlevel",
    "driveon", "drvtype", "drive", "tone", "drvlevel",
    "crushon", "bits", "rate", "compand", "cnoise", "lowshelf", "highshelf", "cmix",
    "lfo1rate", "lfo1amt", "lfo2rate", "lfo2amt", "rndrate", "rndamt", "envamt" };

juce::AudioProcessorValueTreeState::ParameterLayout SandVerbProcessor::createLayout()
{
    using P    = juce::AudioParameterFloat;
    using NR   = juce::NormalisableRange<float>;
    using Attr = juce::AudioParameterFloatAttributes;
    auto id = [](const char* s) { return juce::ParameterID { s, 1 }; };

    auto unit = [](const char* suffix, int decimals, float scale = 1.f) {
        return Attr()
            .withStringFromValueFunction([suffix, decimals, scale](float v, int) { return juce::String(v * scale, decimals) + suffix; })
            .withValueFromStringFunction([scale](const juce::String& t) { return t.getFloatValue() / scale; });
    };
    const auto pct   = unit(" %", 0, 100.f);
    const auto ms    = unit(" ms", 0);
    const auto sec   = unit(" s", 1);
    const auto db    = unit(" dB", 1);
    const auto ratio = unit(":1", 1);
    const auto hz    = unit(" Hz", 2);
    const auto fr    = unit("", 1);
    const auto bip   = Attr()   // -100..+100 %
        .withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.f)) + " %"; })
        .withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue() / 100.f; });

    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto on = [&](const char* s, const char* name) {
        l.add(std::make_unique<juce::AudioParameterBool>(id(s), name, true));
    };
    const auto bits = unit(" bit", 1);
    const auto khz  = Attr()
        .withStringFromValueFunction([](float v, int) { return v >= 1000.f ? juce::String(v / 1000.f, 1) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz"; })
        .withValueFromStringFunction([](const juce::String& t) {
            const float v = t.getFloatValue();
            return t.containsIgnoreCase("k") ? v * 1000.f : v; });

    // --- FEEDBACK: длина кадра и память ---
    l.add(std::make_unique<P>(id("frame"), "Frame", NR(5.f, 2000.f, 0.f, 0.4f), 250.f, ms));
    l.add(std::make_unique<juce::AudioParameterBool>(id("sync"), "Tempo Sync", false));
    l.add(std::make_unique<juce::AudioParameterChoice>(id("division"), "Division",
          juce::StringArray { "1/32", "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars" }, 3));
    l.add(std::make_unique<juce::AudioParameterChoice>(id("feel"), "Feel",
          juce::StringArray { "Straight", "Dotted", "Triplet" }, 0));
    l.add(std::make_unique<P>(id("frames"),  "Depth (frames)", NR(1.f, 16.f), 4.f, fr));
    l.add(std::make_unique<P>(id("falloff"), "Age Falloff",    NR(0.f, 1.f), 0.65f, pct));
    on("fbon", "Feedback On");

    // --- SEND: возврат COMP2 -> FEEDBACK, и Fb Out ---
    l.add(std::make_unique<P>(id("send"),    "Send",       NR(0.f, 0.95f), 0.6f, pct));
    on("sendon", "Send On");
    l.add(std::make_unique<P>(id("retout"),  "Fb Out",     NR(0.f, 1.f), 0.55f, pct));

    // --- REVERB ---
    on("verbon", "Reverb On");
    l.add(std::make_unique<juce::AudioParameterChoice>(id("verbtype"), "Reverb Type",
          juce::StringArray { "Fat", "Tape" }, 0));
    l.add(std::make_unique<P>(id("rt60"),    "Decay",      NR(0.2f, 10.f, 0.f, 0.5f), 2.5f, sec));
    l.add(std::make_unique<P>(id("damping"), "Damping",    NR(0.f, 1.f), 0.5f, pct));
    l.add(std::make_unique<P>(id("mix"),     "Mix",        NR(0.f, 1.f), 0.4f, pct));

    // --- компрессоры ---
    on("comp1on", "Comp 1 On");
    l.add(std::make_unique<P>(id("c1thr"),   "C1 Threshold", NR(-48.f, 0.f), -18.f, db));
    l.add(std::make_unique<P>(id("c1ratio"), "C1 Ratio",   NR(1.f, 20.f, 0.f, 0.5f), 4.f, ratio));
    on("comp2on", "Comp 2 On");
    l.add(std::make_unique<P>(id("c2thr"),   "C2 Threshold", NR(-48.f, 0.f), -12.f, db));
    l.add(std::make_unique<P>(id("c2ratio"), "C2 Ratio",   NR(1.f, 20.f, 0.f, 0.5f), 4.f, ratio));

    // --- SAND = DRIVE -> CRUSHER (параллельно FEEDBACK) ---
    on("sandon", "Sand On");
    l.add(std::make_unique<P>(id("sandlevel"), "Sand Level", NR(0.f, 1.f), 0.7f, pct));

    on("driveon", "Drive On");
    l.add(std::make_unique<juce::AudioParameterChoice>(id("drvtype"), "Drive Type",
          juce::StringArray { "Overdrive", "Tube", "Fuzz" }, 0));
    l.add(std::make_unique<P>(id("drive"),    "Drive",     NR(0.f, 40.f), 12.f, db));
    l.add(std::make_unique<P>(id("tone"),     "Tone",      NR(0.f, 1.f), 0.5f, pct));
    l.add(std::make_unique<P>(id("drvlevel"), "Drive Level", NR(-24.f, 6.f), 0.f, db));

    on("crushon", "Crusher On");
    l.add(std::make_unique<P>(id("bits"),      "Bits",       NR(1.f, 16.f), 8.f, bits));
    l.add(std::make_unique<P>(id("rate"),      "Rate",       NR(200.f, 48000.f, 0.f, 0.25f), 11025.f, khz));
    l.add(std::make_unique<P>(id("compand"),   "Companding", NR(0.f, 1.f), 0.3f, pct));
    l.add(std::make_unique<P>(id("cnoise"),    "Noise",      NR(0.f, 1.f), 0.2f, pct));
    l.add(std::make_unique<P>(id("lowshelf"),  "Low Shelf",  NR(-12.f, 12.f), 0.f, db));
    l.add(std::make_unique<P>(id("highshelf"), "High Shelf", NR(-12.f, 12.f), 0.f, db));
    l.add(std::make_unique<P>(id("cmix"),      "Crusher Mix", NR(0.f, 1.f), 1.f, pct));

    // --- MOD: модули, двигающие глубину хранилища ---
    l.add(std::make_unique<P>(id("lfo1rate"), "LFO1 Rate", NR(0.02f, 20.f, 0.f, 0.35f), 0.25f, hz));
    l.add(std::make_unique<P>(id("lfo1amt"),  "LFO1 Amount", NR(-1.f, 1.f), 0.f, bip));
    l.add(std::make_unique<P>(id("lfo2rate"), "LFO2 Rate", NR(0.02f, 20.f, 0.f, 0.35f), 0.7f, hz));
    l.add(std::make_unique<P>(id("lfo2amt"),  "LFO2 Amount", NR(-1.f, 1.f), 0.f, bip));
    l.add(std::make_unique<P>(id("rndrate"),  "Random Rate", NR(0.1f, 20.f, 0.f, 0.4f), 2.f, hz));
    l.add(std::make_unique<P>(id("rndamt"),   "Random Amount", NR(-1.f, 1.f), 0.f, bip));
    l.add(std::make_unique<P>(id("envamt"),   "Envelope Amount", NR(-1.f, 1.f), 0.f, bip));
    return l;
}

void SandVerbProcessor::prepareToPlay(double sampleRate, int)
{
    sr = sampleRate;
    core.prepare(sampleRate, getTotalNumInputChannels());
}

bool SandVerbProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
           && out == layouts.getMainInputChannelSet();
}

// Длина кадра в сэмплах: либо из мс, либо из темпа хоста
int SandVerbProcessor::computeTargetDelaySamples() const
{
    double seconds;

    if (get(P_sync) > 0.5f)
    {
        double bpm = 120.0, barBeats = 4.0;               // запасные значения без хоста

        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                if (auto b = pos->getBpm(); b && *b > 20.0)
                    bpm = *b;
                if (auto ts = pos->getTimeSignature())
                    barBeats = ts->numerator * 4.0 / ts->denominator;
            }

        // длительности в четвертных; отрицательные = в тактах
        static const double beats[] = { 0.125, 0.25, 0.5, 1.0, 2.0, -1.0, -2.0 };
        const int div = juce::jlimit(0, 6, juce::roundToInt(get(P_division)));
        double b = beats[div];
        if (b < 0.0) b = -b * barBeats;

        const int feel = juce::roundToInt(get(P_feel));
        if      (feel == 1) b *= 1.5;                     // dotted
        else if (feel == 2) b *= 2.0 / 3.0;               // triplet

        seconds = b * 60.0 / bpm;
    }
    else
        seconds = get(P_frame) * 0.001;

    return (int)std::lround(seconds * sr);
}

void SandVerbProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto on = [this](ParamIndex i) { return get(i) > 0.5f ? 1.f : 0.f; };

    LoopParams p;
    p.delaySamples = computeTargetDelaySamples();
    p.frames   = get(P_frames);   p.falloff  = get(P_falloff);
    p.send     = get(P_send);     p.retOut   = get(P_retout);
    p.fbOn     = on(P_fbon);      p.sendOn   = on(P_sendon);    p.verbOn   = on(P_verbon);
    p.comp1On  = on(P_comp1on);   p.comp2On  = on(P_comp2on);   p.sandOn   = on(P_sandon);
    p.sandLevel = get(P_sandlevel);
    p.verbType = juce::roundToInt(get(P_verbtype));
    p.rt60     = get(P_rt60);     p.damping  = get(P_damping);  p.mix      = get(P_mix);
    p.c1Thr    = get(P_c1thr);    p.c1Ratio  = get(P_c1ratio);
    p.c2Thr    = get(P_c2thr);    p.c2Ratio  = get(P_c2ratio);

    SandParams& sp = p.sand;
    sp.driveOn = on(P_driveon);   sp.crushOn = on(P_crushon);
    sp.drvType = juce::roundToInt(get(P_drvtype));
    sp.drive   = get(P_drive);    sp.tone    = get(P_tone);     sp.drvLevel = get(P_drvlevel);
    sp.bits    = get(P_bits);     sp.rate    = get(P_rate);     sp.compand = get(P_compand);
    sp.noise   = get(P_cnoise);   sp.low     = get(P_lowshelf); sp.high    = get(P_highshelf);
    sp.cmix    = get(P_cmix);

    p.lfo1Rate = get(P_lfo1rate); p.lfo1Amt  = get(P_lfo1amt);
    p.lfo2Rate = get(P_lfo2rate); p.lfo2Amt  = get(P_lfo2amt);
    p.rndRate  = get(P_rndrate);  p.rndAmt   = get(P_rndamt);
    p.envAmt   = get(P_envamt);
    core.setTarget(p);

    const int numCh = std::min(buffer.getNumChannels(), core.numChannels());
    float* ch[SandVerbCore::kMaxCh] = {};
    for (int c = 0; c < numCh; ++c) ch[c] = buffer.getWritePointer(c);
    core.process(ch, buffer.getNumSamples());

    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear(c, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* SandVerbProcessor::createEditor()
{
    return new SandVerbEditor(*this);
}

void SandVerbProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void SandVerbProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SandVerbProcessor();
}

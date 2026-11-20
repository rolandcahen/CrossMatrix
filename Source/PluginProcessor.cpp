#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace xm;

//==============================================================================
CrossMatrixProcessor::CrossMatrixProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pSnapA      = apvts.getRawParameterValue (ParamID::snapA);
    pSnapB      = apvts.getRawParameterValue (ParamID::snapB);
    pMorph      = apvts.getRawParameterValue (ParamID::morph);
    pTransition = apvts.getRawParameterValue (ParamID::transition);
    pMaster     = apvts.getRawParameterValue (ParamID::master);

    for (int s = 0; s < numSnapshots; ++s)
    {
        identitySnapshot (s);
        snaps[(size_t) s].name = "Snapshot " + juce::String (s + 1);
    }

    for (auto& c : current) c.store (0.0f);
    for (auto& p : inPeak)  p.store (0.0f);
    for (auto& p : outPeak) p.store (0.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout CrossMatrixProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamID::snapA, 1 }, "Snapshot", 1, numSnapshots, 1));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamID::snapB, 1 }, "Morph Target", 1, numSnapshots, 2));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::morph, 1 }, "Morph",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::transition, 1 }, "Transition",
                                                       NormalisableRange<float> (0.0f, 10000.0f, 1.0f, 0.3f), 50.0f,
                                                       AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::master, 1 }, "Master",
                                                       NormalisableRange<float> (-60.0f, 12.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

//==============================================================================
bool CrossMatrixProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled())
        return false;

    return in.size() >= 1 && in.size() <= maxChannels
        && out.size() >= 1 && out.size() <= maxChannels;
}

void CrossMatrixProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    scratch.setSize (maxChannels, juce::jmax (1, samplesPerBlock), false, true, false);
    numIns  = juce::jmin (maxChannels, getTotalNumInputChannels());
    numOuts = juce::jmin (maxChannels, getTotalNumOutputChannels());

    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pMaster->load(), -60.0f));

    // Pas de fondu au démarrage : on part directement de la matrice cible
    lastA = -1;
    lastB = -1;
    const int a = juce::jlimit (0, numSnapshots - 1, (int) pSnapA->load() - 1);
    const int b = juce::jlimit (0, numSnapshots - 1, (int) pSnapB->load() - 1);
    const float m = pMorph->load();

    for (int c = 0; c < numCells; ++c)
    {
        const float ga = snaps[(size_t) a].gain[(size_t) c].load();
        const float gb = snaps[(size_t) b].gain[(size_t) c].load();
        auto& r = ramps[(size_t) c];
        r.cur = r.target = ga + (gb - ga) * m;
        r.step = 0.0f;
        r.left = 0;
        current[(size_t) c].store (r.cur);
    }
    lastA = a;
    lastB = b;
}

void CrossMatrixProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n    = buffer.getNumSamples();
    const int nIn  = juce::jmin (maxChannels, getTotalNumInputChannels(),  buffer.getNumChannels());
    const int nOut = juce::jmin (maxChannels, getTotalNumOutputChannels(), buffer.getNumChannels());
    numIns  = nIn;
    numOuts = nOut;

    if (n == 0)
        return;

    if (scratch.getNumSamples() < n)
        scratch.setSize (maxChannels, n, false, false, true);

    // 1. Copie des entrées (le buffer est traité en place)
    for (int i = 0; i < nIn; ++i)
    {
        scratch.copyFrom (i, 0, buffer, i, 0, n);
        const float pk = buffer.getMagnitude (i, 0, n);
        auto& p = inPeak[(size_t) i];
        if (pk > p.load (std::memory_order_relaxed)) p.store (pk, std::memory_order_relaxed);
    }

    // 2. Calcul de la matrice cible (snapshot A -> B selon le morph)
    const int a = juce::jlimit (0, numSnapshots - 1, (int) pSnapA->load() - 1);
    const int b = juce::jlimit (0, numSnapshots - 1, (int) pSnapB->load() - 1);
    const float m = juce::jlimit (0.0f, 1.0f, pMorph->load());
    const bool snapChanged = (a != lastA || b != lastB);
    lastA = a;
    lastB = b;

    const int transitionSamples = juce::jmax (1, (int) (pTransition->load() * 0.001 * sr));
    const int shortRamp         = juce::jmax (1, (int) (0.01 * sr));   // 10 ms pour les éditions et le morph

    for (int c = 0; c < numCells; ++c)
    {
        const float ga = snaps[(size_t) a].gain[(size_t) c].load (std::memory_order_relaxed);
        const float gb = snaps[(size_t) b].gain[(size_t) c].load (std::memory_order_relaxed);
        const float t  = ga + (gb - ga) * m;
        auto& r = ramps[(size_t) c];

        if (std::abs (t - r.target) > 1.0e-7f)
        {
            int len = snapChanged ? transitionSamples : juce::jmax (shortRamp, r.left);
            r.target = t;
            r.left = len;
            r.step = (t - r.cur) / (float) len;
        }
    }

    // 3. Mixage : chaque sortie = somme pondérée des entrées
    for (int o = 0; o < nOut; ++o)
        buffer.clear (o, 0, n);
    for (int o = nOut; o < buffer.getNumChannels(); ++o)
        buffer.clear (o, 0, n);

    for (int i = 0; i < maxChannels; ++i)
    {
        for (int o = 0; o < maxChannels; ++o)
        {
            auto& r = ramps[(size_t) cellIndex (i, o)];
            const float g0 = r.cur;

            if (r.left > 0)
            {
                const int k = juce::jmin (r.left, n);
                r.left -= k;
                r.cur = (r.left == 0) ? r.target : r.cur + r.step * (float) k;
            }
            const float g1 = r.cur;
            current[(size_t) cellIndex (i, o)].store (g1, std::memory_order_relaxed);

            if (i >= nIn || o >= nOut)
                continue;
            if (g0 == 0.0f && g1 == 0.0f)
                continue;

            if (g0 == g1)
                buffer.addFrom (o, 0, scratch, i, 0, n, g1);
            else
                buffer.addFromWithRamp (o, 0, scratch.getReadPointer (i), n, g0, g1);
        }
    }

    // 4. Master + mesures
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (pMaster->load(), -60.0f));
    if (masterGain.isSmoothing())
    {
        const float g0 = masterGain.getCurrentValue();
        masterGain.skip (n);
        const float g1 = masterGain.getCurrentValue();
        for (int o = 0; o < nOut; ++o)
            buffer.applyGainRamp (o, 0, n, g0, g1);
    }
    else if (masterGain.getTargetValue() != 1.0f)
    {
        for (int o = 0; o < nOut; ++o)
            buffer.applyGain (o, 0, n, masterGain.getTargetValue());
    }

    for (int o = 0; o < nOut; ++o)
    {
        const float pk = buffer.getMagnitude (o, 0, n);
        auto& p = outPeak[(size_t) o];
        if (pk > p.load (std::memory_order_relaxed)) p.store (pk, std::memory_order_relaxed);
    }
}

//==============================================================================
int CrossMatrixProcessor::getSnapA() const noexcept
{
    return juce::jlimit (0, numSnapshots - 1, (int) pSnapA->load() - 1);
}

void CrossMatrixProcessor::selectSnapshot (int index)
{
    if (auto* p = apvts.getParameter (ParamID::snapA))
    {
        index = juce::jlimit (0, numSnapshots - 1, index);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (index + 1)));
        p->endChangeGesture();
    }
}

float CrossMatrixProcessor::getCell (int snap, int in, int out) const noexcept
{
    return snaps[(size_t) snap].gain[(size_t) cellIndex (in, out)].load (std::memory_order_relaxed);
}

void CrossMatrixProcessor::setCell (int snap, int in, int out, float g) noexcept
{
    snaps[(size_t) snap].gain[(size_t) cellIndex (in, out)].store (juce::jlimit (0.0f, 4.0f, g), std::memory_order_relaxed);
}

juce::String CrossMatrixProcessor::getSnapshotName (int snap) const
{
    const juce::ScopedLock sl (nameLock);
    return snaps[(size_t) juce::jlimit (0, numSnapshots - 1, snap)].name;
}

void CrossMatrixProcessor::setSnapshotName (int snap, const juce::String& n)
{
    {
        const juce::ScopedLock sl (nameLock);
        snaps[(size_t) juce::jlimit (0, numSnapshots - 1, snap)].name = n;
    }
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void CrossMatrixProcessor::clearSnapshot (int snap)
{
    for (auto& g : snaps[(size_t) snap].gain)
        g.store (0.0f);
}

void CrossMatrixProcessor::identitySnapshot (int snap)
{
    clearSnapshot (snap);
    for (int c = 0; c < maxChannels; ++c)
        setCell (snap, c, c, 1.0f);
}

void CrossMatrixProcessor::copySnapshotToClipboard (int snap)
{
    for (int c = 0; c < numCells; ++c)
        clipboard[(size_t) c] = snaps[(size_t) snap].gain[(size_t) c].load();
    clipboardName = getSnapshotName (snap);
    clipboardValid = true;
}

void CrossMatrixProcessor::pasteClipboardToSnapshot (int snap)
{
    if (! clipboardValid)
        return;
    for (int c = 0; c < numCells; ++c)
        snaps[(size_t) snap].gain[(size_t) c].store (clipboard[(size_t) c]);
}

//==============================================================================
// Sérialisation : seules les cellules non nulles sont stockées ("in:out:gain")
juce::ValueTree CrossMatrixProcessor::snapshotToTree (int snap) const
{
    juce::ValueTree t ("SNAPSHOT");
    t.setProperty ("index", snap + 1, nullptr);
    t.setProperty ("name", getSnapshotName (snap), nullptr);

    juce::StringArray cells;
    for (int i = 0; i < maxChannels; ++i)
        for (int o = 0; o < maxChannels; ++o)
            if (const float g = getCell (snap, i, o); g != 0.0f)
                cells.add (juce::String (i + 1) + ":" + juce::String (o + 1) + ":" + juce::String (g, 6));

    t.setProperty ("cells", cells.joinIntoString (" "), nullptr);
    return t;
}

void CrossMatrixProcessor::snapshotFromTree (int snap, const juce::ValueTree& t)
{
    clearSnapshot (snap);
    if (t.hasProperty ("name"))
        setSnapshotName (snap, t["name"].toString());

    for (auto& token : juce::StringArray::fromTokens (t["cells"].toString(), " ", ""))
    {
        auto parts = juce::StringArray::fromTokens (token, ":", "");
        if (parts.size() != 3)
            continue;
        const int i = parts[0].getIntValue() - 1;
        const int o = parts[1].getIntValue() - 1;
        if (juce::isPositiveAndBelow (i, maxChannels) && juce::isPositiveAndBelow (o, maxChannels))
            setCell (snap, i, o, parts[2].getFloatValue());
    }
}

juce::ValueTree CrossMatrixProcessor::bankToTree() const
{
    juce::ValueTree bank ("SNAPSHOTS");
    for (int s = 0; s < numSnapshots; ++s)
        bank.appendChild (snapshotToTree (s), nullptr);
    return bank;
}

void CrossMatrixProcessor::bankFromTree (const juce::ValueTree& bank)
{
    for (const auto& child : bank)
    {
        if (! child.hasType ("SNAPSHOT"))
            continue;
        const int idx = (int) child["index"] - 1;
        if (juce::isPositiveAndBelow (idx, numSnapshots))
            snapshotFromTree (idx, child);
    }
    ++uiStamp;
}

void CrossMatrixProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("CROSSMATRIX");
    root.setProperty ("version", 1, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (bankToTree(), nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void CrossMatrixProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("CROSSMATRIX"))
        return;

    bankFromTree (root.getChildWithName ("SNAPSHOTS"));

    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid())
        apvts.replaceState (params);
    ++uiStamp;
}

//==============================================================================
static bool writeTree (const juce::ValueTree& t, const juce::File& f)
{
    if (auto xml = t.createXml())
        return xml->writeTo (f);
    return false;
}

static juce::ValueTree readTree (const juce::File& f)
{
    if (auto xml = juce::XmlDocument::parse (f))
        return juce::ValueTree::fromXml (*xml);
    return {};
}

bool CrossMatrixProcessor::saveSnapshotFile (int snap, const juce::File& f) const
{
    return writeTree (snapshotToTree (snap), f);
}

bool CrossMatrixProcessor::loadSnapshotFile (int snap, const juce::File& f)
{
    auto t = readTree (f);
    if (t.hasType ("SNAPSHOTS"))           // une banque : on prend le premier snapshot
        t = t.getChild (0);
    if (! t.hasType ("SNAPSHOT"))
        return false;
    snapshotFromTree (snap, t);
    ++uiStamp;
    return true;
}

bool CrossMatrixProcessor::saveBankFile (const juce::File& f) const
{
    return writeTree (bankToTree(), f);
}

bool CrossMatrixProcessor::loadBankFile (const juce::File& f)
{
    auto t = readTree (f);
    if (! t.hasType ("SNAPSHOTS"))
        return false;
    bankFromTree (t);
    return true;
}

//==============================================================================
juce::AudioProcessorEditor* CrossMatrixProcessor::createEditor()
{
    return new CrossMatrixEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrossMatrixProcessor();
}

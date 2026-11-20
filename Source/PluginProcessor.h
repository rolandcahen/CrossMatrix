#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace xm
{
    constexpr int maxChannels  = 16;   // entrées et sorties max
    constexpr int numSnapshots = 16;   // presets internes automatisables
    constexpr int numCells     = maxChannels * maxChannels;

    inline int cellIndex (int in, int out) noexcept { return in * maxChannels + out; }

    namespace ParamID
    {
        inline constexpr const char* snapA      = "snapA";
        inline constexpr const char* snapB      = "snapB";
        inline constexpr const char* morph      = "morph";
        inline constexpr const char* transition = "transition";
        inline constexpr const char* master     = "master";
    }
}

//==============================================================================
class CrossMatrixProcessor : public juce::AudioProcessor
{
public:
    CrossMatrixProcessor();
    ~CrossMatrixProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Les "programmes" de l'hôte = les 16 snapshots
    int getNumPrograms() override { return xm::numSnapshots; }
    int getCurrentProgram() override { return getSnapA(); }
    void setCurrentProgram (int index) override { selectSnapshot (index); }
    const juce::String getProgramName (int index) override { return getSnapshotName (index); }
    void changeProgramName (int index, const juce::String& n) override { setSnapshotName (index, n); }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // API utilisée par l'éditeur (thread message)
    juce::AudioProcessorValueTreeState apvts;

    int   getSnapA() const noexcept;
    void  selectSnapshot (int index);

    float getCell (int snap, int in, int out) const noexcept;
    void  setCell (int snap, int in, int out, float linearGain) noexcept;
    float getEffectiveGain (int in, int out) const noexcept { return current[(size_t) xm::cellIndex (in, out)].load (std::memory_order_relaxed); }

    juce::String getSnapshotName (int snap) const;
    void         setSnapshotName (int snap, const juce::String&);

    void clearSnapshot (int snap);
    void identitySnapshot (int snap);
    void copySnapshotToClipboard (int snap);
    void pasteClipboardToSnapshot (int snap);
    bool hasClipboard() const noexcept { return clipboardValid; }

    bool saveSnapshotFile (int snap, const juce::File&) const;
    bool loadSnapshotFile (int snap, const juce::File&);
    bool saveBankFile (const juce::File&) const;
    bool loadBankFile (const juce::File&);

    int   getNumIns()  const noexcept { return numIns.load(); }
    int   getNumOuts() const noexcept { return numOuts.load(); }
    float popInputPeak (int ch) noexcept  { return inPeak[(size_t) ch].exchange (0.0f); }
    float popOutputPeak (int ch) noexcept { return outPeak[(size_t) ch].exchange (0.0f); }

    std::atomic<int> uiStamp { 0 };   // incrémenté quand un snapshot change hors UI (chargement…)

private:
    struct Snapshot
    {
        Snapshot() { for (auto& g : gain) g.store (0.0f); }
        std::array<std::atomic<float>, xm::numCells> gain;
        juce::String name;
    };

    struct Ramp
    {
        float cur = 0.0f, target = 0.0f, step = 0.0f;
        int left = 0;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::ValueTree snapshotToTree (int snap) const;
    void snapshotFromTree (int snap, const juce::ValueTree&);
    juce::ValueTree bankToTree() const;
    void bankFromTree (const juce::ValueTree&);

    std::array<Snapshot, xm::numSnapshots> snaps;
    mutable juce::CriticalSection nameLock;

    std::array<float, xm::numCells> clipboard {};
    juce::String clipboardName;
    bool clipboardValid = false;

    std::array<Ramp, xm::numCells> ramps;
    std::array<std::atomic<float>, xm::numCells> current;
    juce::AudioBuffer<float> scratch;
    juce::SmoothedValue<float> masterGain;

    std::atomic<float>* pSnapA = nullptr;
    std::atomic<float>* pSnapB = nullptr;
    std::atomic<float>* pMorph = nullptr;
    std::atomic<float>* pTransition = nullptr;
    std::atomic<float>* pMaster = nullptr;

    int lastA = -1, lastB = -1;
    double sr = 44100.0;

    std::atomic<int> numIns { 2 }, numOuts { 2 };
    std::array<std::atomic<float>, xm::maxChannels> inPeak, outPeak;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrossMatrixProcessor)
};

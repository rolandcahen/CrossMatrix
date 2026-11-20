#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

//==============================================================================
// Grille : entrées en lignes (ordonnées), sorties en colonnes (abscisse)
class MatrixGrid : public juce::Component
{
public:
    explicit MatrixGrid (CrossMatrixProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    std::array<float, xm::maxChannels> inLevels {}, outLevels {};

private:
    struct Geometry { int nIn, nOut; float cell; juce::Rectangle<float> area; };
    Geometry geometry() const;
    bool hitCell (juce::Point<float>, int& in, int& out) const;
    juce::String channelName (bool isInput, int index) const;
    void showCellMenu (int in, int out);
    void showHeaderMenu (bool isInput, int index);

    CrossMatrixProcessor& proc;
    int dragIn = -1, dragOut = -1, hoverIn = -1, hoverOut = -1;
    float dragStartDb = 0.0f;
    bool dragged = false, wasOff = false;

    static constexpr float rowHeaderW = 92.0f, colHeaderH = 62.0f;
};

//==============================================================================
class CrossMatrixEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit CrossMatrixEditor (CrossMatrixProcessor&);
    ~CrossMatrixEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshSnapshotNames();
    void chooseFile (bool save, bool bank);

    CrossMatrixProcessor& proc;
    MatrixGrid grid;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    juce::Label snapLabel { {}, "Snapshot" }, targetLabel { {}, "Morph vers" };
    juce::ComboBox snapBox, targetBox;
    juce::TextEditor nameEditor;
    juce::TextButton copyBtn { "Copier" }, pasteBtn { "Coller" }, clearBtn { "Effacer" }, identityBtn { "Identite" };
    juce::TextButton loadSnapBtn { "Ouvrir snapshot..." }, saveSnapBtn { "Enregistrer snapshot..." },
                     loadBankBtn { "Ouvrir banque..." },   saveBankBtn { "Enregistrer banque..." };

    juce::Slider morphSlider, transitionSlider, masterSlider;
    juce::Label morphLabel { {}, "Morph" }, transitionLabel { {}, "Transition" }, masterLabel { {}, "Master" };

    std::unique_ptr<ComboAttachment> snapAtt, targetAtt;
    std::unique_ptr<SliderAttachment> morphAtt, transitionAtt, masterAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Label status;
    int lastSnap = -1, lastStamp = -1, lastIns = -1, lastOuts = -1;

    juce::LookAndFeel_V4 lnf { juce::LookAndFeel_V4::getMidnightColourScheme() };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrossMatrixEditor)
};

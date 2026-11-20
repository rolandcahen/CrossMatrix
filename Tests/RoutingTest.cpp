#include "../Source/PluginProcessor.h"
#include <cstdio>
static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "OK  " : "FAIL", what); if (! ok) ++fails; }
static bool near (float a, float b) { return std::abs (a - b) < 1e-4f; }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    CrossMatrixProcessor p;
    juce::AudioProcessor::BusesLayout l;
    l.inputBuses.add (juce::AudioChannelSet::discreteChannels (4));
    l.outputBuses.add (juce::AudioChannelSet::discreteChannels (4));
    check (p.setBusesLayout (l), "layout 4 in / 4 out");
    p.prepareToPlay (48000, 512);
    juce::AudioBuffer<float> buf (4, 512); juce::MidiBuffer midi;
    auto fill = [&] { for (int c = 0; c < 4; ++c) for (int s = 0; s < 512; ++s) buf.setSample (c, s, 0.1f * (float) (c + 1)); };

    // Snapshot 1 : in1 -> out1,out2,out3 (division) ; in2+in3+in4 -> out4 (mélange)
    p.clearSnapshot (0);
    p.setCell (0, 0, 0, 1); p.setCell (0, 0, 1, 1); p.setCell (0, 0, 2, 0.5f);
    p.setCell (0, 1, 3, 1); p.setCell (0, 2, 3, 1); p.setCell (0, 3, 3, 1);
    for (int k = 0; k < 10; ++k) { fill(); p.processBlock (buf, midi); }
    check (near (buf.getSample (0, 511), 0.1f), "division in1 -> out1");
    check (near (buf.getSample (1, 511), 0.1f), "division in1 -> out2");
    check (near (buf.getSample (2, 511), 0.05f), "division in1 -> out3 a -6 dB");
    check (near (buf.getSample (3, 511), 0.2f + 0.3f + 0.4f), "melange in2+in3+in4 -> out4");

    // Snapshot 2 : identité ; passage via le paramètre avec transition de 100 ms
    p.identitySnapshot (1);
    p.apvts.getParameter ("transition")->setValueNotifyingHost (p.apvts.getParameter ("transition")->convertTo0to1 (100.0f));
    p.apvts.getParameter ("snapA")->setValueNotifyingHost (p.apvts.getParameter ("snapA")->convertTo0to1 (2.0f));
    fill(); p.processBlock (buf, midi);
    const float mid = buf.getSample (3, 511);   // ~10.7 ms dans une rampe de 100 ms
    check (mid > 0.4f && mid < 0.9f, "transition progressive (pas de saut)");
    for (int k = 0; k < 12; ++k) { fill(); p.processBlock (buf, midi); }
    check (near (buf.getSample (3, 511), 0.4f) && near (buf.getSample (1, 511), 0.2f), "snapshot 2 atteint (identite)");

    // Morph 50 % entre snapshot 2 et 1
    p.apvts.getParameter ("snapB")->setValueNotifyingHost (p.apvts.getParameter ("snapB")->convertTo0to1 (1.0f));
    p.apvts.getParameter ("morph")->setValueNotifyingHost (0.5f);
    for (int k = 0; k < 200; ++k) { fill(); p.processBlock (buf, midi); }
    check (near (buf.getSample (3, 511), 0.5f * 0.4f + 0.5f * 0.9f), "morph 50 %");

    // Sauvegarde / restauration d'état
    juce::MemoryBlock mb; p.getStateInformation (mb);
    CrossMatrixProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
    check (near (q.getCell (0, 0, 2), 0.5f) && near (q.getCell (0, 3, 3), 1.0f) && q.getSnapA() == 1, "etat sauve/restaure");
    if (auto* ed = q.createEditor())
    {
        ed->setSize (900, 680);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile ("editor.png"));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        delete ed;
    }
    std::printf (fails ? "\n%d ECHEC(S)\n" : "\nTOUS LES TESTS PASSENT\n", fails);
    return fails;
}

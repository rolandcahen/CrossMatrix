#include "PluginEditor.h"

using namespace xm;

namespace
{
    juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    constexpr float minDb = -60.0f, maxDb = 12.0f;

    float gainToDb (float g) { return g > 0.0f ? juce::Decibels::gainToDecibels (g, -100.0f) : -100.0f; }
    float dbToGain (float db) { return db <= minDb ? 0.0f : juce::Decibels::decibelsToGain (db); }

    juce::String dbText (float g)
    {
        if (g <= 0.0f) return "-inf";
        const float db = gainToDb (g);
        if (std::abs (db) < 0.05f) return "0";
        return juce::String (db, std::abs (db) >= 10.0f ? 0 : 1);
    }

    const juce::Colour bg       { 0xff15181d };
    const juce::Colour panel    { 0xff1d2128 };
    const juce::Colour gridLine { 0xff2c323b };
    const juce::Colour accent   { 0xff3fd0a0 };
    const juce::Colour hot      { 0xffffa63f };
    const juce::Colour textCol  { 0xffd8dde4 };
}

//==============================================================================
MatrixGrid::MatrixGrid (CrossMatrixProcessor& p) : proc (p)
{
    setRepaintsOnMouseActivity (false);
}

MatrixGrid::Geometry MatrixGrid::geometry() const
{
    Geometry g;
    g.nIn  = juce::jmax (1, proc.getNumIns());
    g.nOut = juce::jmax (1, proc.getNumOuts());
    const float w = (float) getWidth()  - rowHeaderW - 4.0f;
    const float h = (float) getHeight() - colHeaderH - 4.0f;
    g.cell = juce::jlimit (12.0f, 64.0f, juce::jmin (w / (float) g.nOut, h / (float) g.nIn));
    g.area = { rowHeaderW, colHeaderH, g.cell * (float) g.nOut, g.cell * (float) g.nIn };
    return g;
}

bool MatrixGrid::hitCell (juce::Point<float> pt, int& in, int& out) const
{
    const auto g = geometry();
    if (! g.area.contains (pt))
        return false;
    out = juce::jlimit (0, g.nOut - 1, (int) ((pt.x - g.area.getX()) / g.cell));
    in  = juce::jlimit (0, g.nIn  - 1, (int) ((pt.y - g.area.getY()) / g.cell));
    return true;
}

juce::String MatrixGrid::channelName (bool isInput, int index) const
{
    juce::String s (index + 1);
    if (auto* bus = proc.getBus (isInput, 0))
    {
        const auto layout = bus->getCurrentLayout();
        if (! layout.isDiscreteLayout() && index < layout.size())
        {
            const auto abbr = juce::AudioChannelSet::getAbbreviatedChannelTypeName (layout.getTypeOfChannel (index));
            if (abbr.isNotEmpty())
                s << " " << abbr;
        }
    }
    return s;
}

void MatrixGrid::paint (juce::Graphics& gfx)
{
    const auto g = geometry();
    const int snap = proc.getSnapA();

    // Titres des axes
    gfx.setColour (textCol.withAlpha (0.55f));
    gfx.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    gfx.drawText (T ("SORTIES \xe2\x86\x92"), juce::Rectangle<float> (g.area.getX(), 0.0f, 200.0f, 14.0f), juce::Justification::centredLeft);
    gfx.drawText (T ("ENTR\xc3\x89" "ES \xe2\x86\x93"), juce::Rectangle<float> (0.0f, g.area.getY() - 16.0f, rowHeaderW, 14.0f), juce::Justification::centredLeft);

    // En-têtes de colonnes (sorties) avec vumètre
    for (int o = 0; o < g.nOut; ++o)
    {
        auto r = juce::Rectangle<float> (g.area.getX() + g.cell * (float) o, 16.0f, g.cell, colHeaderH - 20.0f).reduced (1.5f, 0.0f);
        gfx.setColour (o == hoverOut ? panel.brighter (0.15f) : panel);
        gfx.fillRoundedRectangle (r, 3.0f);

        const float lvl = juce::jlimit (0.0f, 1.0f, juce::jmap (juce::Decibels::gainToDecibels (outLevels[(size_t) o], -60.0f), -60.0f, 0.0f, 0.0f, 1.0f));
        auto meter = r.removeFromBottom (5.0f).reduced (2.0f, 1.0f);
        gfx.setColour (outLevels[(size_t) o] > 1.0f ? juce::Colours::red : accent);
        gfx.fillRect (meter.withWidth (meter.getWidth() * lvl));

        gfx.setColour (textCol);
        gfx.setFont (juce::FontOptions (juce::jmin (12.0f, g.cell * 0.36f)));
        gfx.drawFittedText (channelName (false, o), r.toNearestInt(), juce::Justification::centred, 2);
    }

    // En-têtes de lignes (entrées) avec vumètre
    for (int i = 0; i < g.nIn; ++i)
    {
        auto r = juce::Rectangle<float> (0.0f, g.area.getY() + g.cell * (float) i, rowHeaderW - 4.0f, g.cell).reduced (0.0f, 1.5f);
        gfx.setColour (i == hoverIn ? panel.brighter (0.15f) : panel);
        gfx.fillRoundedRectangle (r, 3.0f);

        const float lvl = juce::jlimit (0.0f, 1.0f, juce::jmap (juce::Decibels::gainToDecibels (inLevels[(size_t) i], -60.0f), -60.0f, 0.0f, 0.0f, 1.0f));
        auto meter = r.removeFromRight (6.0f).reduced (1.0f, 2.0f);
        gfx.setColour (inLevels[(size_t) i] > 1.0f ? juce::Colours::red : accent);
        gfx.fillRect (meter.withTop (meter.getBottom() - meter.getHeight() * lvl));

        gfx.setColour (textCol);
        gfx.setFont (juce::FontOptions (juce::jmin (12.0f, g.cell * 0.4f)));
        gfx.drawFittedText ("In " + channelName (true, i), r.reduced (4.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1);
    }

    // Cellules
    for (int i = 0; i < g.nIn; ++i)
    {
        for (int o = 0; o < g.nOut; ++o)
        {
            auto r = juce::Rectangle<float> (g.area.getX() + g.cell * (float) o, g.area.getY() + g.cell * (float) i, g.cell, g.cell);
            const bool hover = (i == hoverIn || o == hoverOut);
            gfx.setColour (hover ? panel.brighter (0.08f) : panel);
            gfx.fillRect (r.reduced (0.5f));

            const float gain = proc.getCell (snap, i, o);
            const auto inner = r.reduced (juce::jmax (2.0f, g.cell * 0.1f));

            if (gain > 0.0f)
            {
                const float db = gainToDb (gain);
                const float t  = juce::jlimit (0.0f, 1.0f, juce::jmap (db, minDb, 0.0f, 0.15f, 1.0f));
                const auto col = db > 0.05f ? hot : accent;
                gfx.setColour (col.withAlpha (0.25f + 0.75f * t));
                gfx.fillRoundedRectangle (inner, 3.0f);

                if (g.cell >= 26.0f)
                {
                    gfx.setColour (juce::Colours::black.withAlpha (0.8f));
                    gfx.setFont (juce::FontOptions (juce::jmin (12.0f, g.cell * 0.3f), juce::Font::bold));
                    gfx.drawText (dbText (gain), inner, juce::Justification::centred);
                }
            }

            // Gain effectif réel (morph / transition en cours) : petite barre en bas de la cellule
            const float eff = proc.getEffectiveGain (i, o);
            if (eff > 0.0f || gain > 0.0f)
            {
                const float e = eff <= 0.0f ? 0.0f : juce::jlimit (0.0f, 1.0f, juce::jmap (gainToDb (eff), minDb, maxDb, 0.0f, 1.0f));
                auto bar = r.reduced (3.0f).removeFromBottom (2.0f);
                gfx.setColour (juce::Colours::white.withAlpha (0.65f));
                gfx.fillRect (bar.withWidth (bar.getWidth() * e));
            }
        }
    }

    // Lignes de la grille
    gfx.setColour (gridLine);
    for (int o = 0; o <= g.nOut; ++o)
        gfx.drawVerticalLine (juce::roundToInt (g.area.getX() + g.cell * (float) o), g.area.getY(), g.area.getBottom());
    for (int i = 0; i <= g.nIn; ++i)
        gfx.drawHorizontalLine (juce::roundToInt (g.area.getY() + g.cell * (float) i), g.area.getX(), g.area.getRight());

    // Info-bulle de la cellule survolée
    if (hoverIn >= 0 && hoverOut >= 0)
    {
        const auto txt = "In " + channelName (true, hoverIn) + T ("  \xe2\x86\x92  Out ") + channelName (false, hoverOut)
                       + "   " + dbText (proc.getCell (snap, hoverIn, hoverOut)) + " dB";
        gfx.setColour (textCol.withAlpha (0.8f));
        gfx.setFont (juce::FontOptions (12.0f));
        gfx.drawText (txt, juce::Rectangle<float> (g.area.getX(), g.area.getBottom() + 2.0f, 400.0f, 16.0f), juce::Justification::centredLeft);
    }
}

void MatrixGrid::mouseMove (const juce::MouseEvent& e)
{
    int i = -1, o = -1;
    const auto g = geometry();
    if (! hitCell (e.position, i, o))
    {
        i = o = -1;
        // survol des en-têtes
        if (e.position.x < rowHeaderW && e.position.y >= g.area.getY() && e.position.y < g.area.getBottom())
            i = (int) ((e.position.y - g.area.getY()) / g.cell);
        if (e.position.y < colHeaderH && e.position.x >= g.area.getX() && e.position.x < g.area.getRight())
            o = (int) ((e.position.x - g.area.getX()) / g.cell);
    }
    if (i != hoverIn || o != hoverOut)
    {
        hoverIn = i;
        hoverOut = o;
        repaint();
    }
}

void MatrixGrid::mouseExit (const juce::MouseEvent&)
{
    hoverIn = hoverOut = -1;
    repaint();
}

void MatrixGrid::mouseDown (const juce::MouseEvent& e)
{
    dragIn = dragOut = -1;
    dragged = false;

    int i, o;
    if (! hitCell (e.position, i, o))
    {
        if (e.mods.isPopupMenu() || e.mods.isLeftButtonDown())
        {
            const auto g = geometry();
            if (e.position.x < rowHeaderW && e.position.y >= g.area.getY() && e.position.y < g.area.getBottom())
                showHeaderMenu (true, (int) ((e.position.y - g.area.getY()) / g.cell));
            else if (e.position.y < colHeaderH && e.position.x >= g.area.getX() && e.position.x < g.area.getRight())
                showHeaderMenu (false, (int) ((e.position.x - g.area.getX()) / g.cell));
        }
        return;
    }

    if (e.mods.isPopupMenu())
    {
        showCellMenu (i, o);
        return;
    }

    dragIn = i;
    dragOut = o;
    const float gain = proc.getCell (proc.getSnapA(), i, o);
    wasOff = gain <= 0.0f;
    dragStartDb = wasOff ? -24.0f : gainToDb (gain);
}

void MatrixGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragIn < 0)
        return;
    if (! dragged && e.getDistanceFromDragStart() < 3)
        return;
    dragged = true;

    const float sensitivity = e.mods.isShiftDown() ? 0.05f : 0.3f;   // dB par pixel
    const float db = juce::jlimit (minDb, maxDb, dragStartDb - (float) e.getDistanceFromDragStartY() * sensitivity);
    proc.setCell (proc.getSnapA(), dragIn, dragOut, dbToGain (db));
    repaint();
}

void MatrixGrid::mouseUp (const juce::MouseEvent&)
{
    if (dragIn >= 0 && ! dragged)
    {
        const int snap = proc.getSnapA();
        proc.setCell (snap, dragIn, dragOut, wasOff ? 1.0f : 0.0f);
        repaint();
    }
    dragIn = dragOut = -1;
}

void MatrixGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    int i, o;
    if (hitCell (e.position, i, o) && ! e.mods.isPopupMenu())
    {
        proc.setCell (proc.getSnapA(), i, o, 1.0f);   // retour à 0 dB
        repaint();
    }
}

void MatrixGrid::showCellMenu (int in, int out)
{
    juce::PopupMenu m;
    m.addSectionHeader ("In " + channelName (true, in) + T (" \xe2\x86\x92 Out ") + channelName (false, out));
    const float values[] = { 6.0f, 3.0f, 0.0f, -3.0f, -4.5f, -6.0f, -9.0f, -12.0f, -18.0f, -24.0f };
    int id = 1;
    for (auto v : values)
        m.addItem (id++, (v > 0 ? "+" : "") + juce::String (v, v == (int) v ? 0 : 1) + " dB");
    m.addSeparator();
    m.addItem (100, "Couper");

    juce::Component::SafePointer<MatrixGrid> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe, in, out, values] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         const int snap = safe->proc.getSnapA();
                         if (r == 100) safe->proc.setCell (snap, in, out, 0.0f);
                         else          safe->proc.setCell (snap, in, out, dbToGain (values[r - 1]));
                         safe->repaint();
                     });
}

void MatrixGrid::showHeaderMenu (bool isInput, int index)
{
    const auto g = geometry();
    const int count = isInput ? g.nOut : g.nIn;

    juce::PopupMenu m;
    if (isInput)
    {
        m.addSectionHeader ("In " + channelName (true, index));
        m.addItem (1, T ("Diviser vers toutes les sorties (0 dB)"));
        m.addItem (2, T ("Diviser vers toutes les sorties (-3 dB)"));
        m.addItem (3, T ("Couper toute la ligne"));
    }
    else
    {
        m.addSectionHeader ("Out " + channelName (false, index));
        m.addItem (1, T ("M\xc3\xa9langer toutes les entr\xc3\xa9" "es (0 dB)"));
        m.addItem (2, T ("M\xc3\xa9langer toutes les entr\xc3\xa9" "es (compens\xc3\xa9 : -10\xc2\xb7log N dB)"));
        m.addItem (3, T ("Couper toute la colonne"));
    }

    juce::Component::SafePointer<MatrixGrid> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe, isInput, index, count] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         float gain = 0.0f;
                         if (r == 1) gain = 1.0f;
                         if (r == 2) gain = isInput ? dbToGain (-3.0f) : 1.0f / std::sqrt ((float) juce::jmax (1, count));
                         const int snap = safe->proc.getSnapA();
                         for (int k = 0; k < count; ++k)
                         {
                             if (isInput) safe->proc.setCell (snap, index, k, gain);
                             else         safe->proc.setCell (snap, k, index, gain);
                         }
                         safe->repaint();
                     });
}

//==============================================================================
CrossMatrixEditor::CrossMatrixEditor (CrossMatrixProcessor& p)
    : AudioProcessorEditor (&p), proc (p), grid (p)
{
    setLookAndFeel (&lnf);
    lnf.setColour (juce::ResizableWindow::backgroundColourId, bg);

    identityBtn.setButtonText (T ("Identit\xc3\xa9"));

    for (int s = 0; s < numSnapshots; ++s)
    {
        snapBox.addItem (juce::String (s + 1), s + 1);
        targetBox.addItem (juce::String (s + 1), s + 1);
    }
    refreshSnapshotNames();

    for (auto* c : std::initializer_list<juce::Component*> { &snapLabel, &targetLabel, &snapBox, &targetBox, &nameEditor,
                                                             &copyBtn, &pasteBtn, &clearBtn, &identityBtn,
                                                             &loadSnapBtn, &saveSnapBtn, &loadBankBtn, &saveBankBtn,
                                                             &morphSlider, &transitionSlider, &masterSlider,
                                                             &morphLabel, &transitionLabel, &masterLabel, &status, &grid })
        addAndMakeVisible (c);

    for (auto* s : { &morphSlider, &transitionSlider, &masterSlider })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 22);
        s->setColour (juce::Slider::trackColourId, accent.withAlpha (0.7f));
    }

    snapAtt       = std::make_unique<ComboAttachment>  (proc.apvts, ParamID::snapA, snapBox);
    targetAtt     = std::make_unique<ComboAttachment>  (proc.apvts, ParamID::snapB, targetBox);
    morphAtt      = std::make_unique<SliderAttachment> (proc.apvts, ParamID::morph, morphSlider);
    transitionAtt = std::make_unique<SliderAttachment> (proc.apvts, ParamID::transition, transitionSlider);
    masterAtt     = std::make_unique<SliderAttachment> (proc.apvts, ParamID::master, masterSlider);

    nameEditor.setTextToShowWhenEmpty ("Nom du snapshot", juce::Colours::grey);
    nameEditor.onTextChange = [this]
    {
        proc.setSnapshotName (proc.getSnapA(), nameEditor.getText());
        refreshSnapshotNames();
    };

    copyBtn.onClick     = [this] { proc.copySnapshotToClipboard (proc.getSnapA()); status.setText (T ("Snapshot copi\xc3\xa9"), juce::dontSendNotification); };
    pasteBtn.onClick    = [this] { proc.pasteClipboardToSnapshot (proc.getSnapA()); grid.repaint(); };
    clearBtn.onClick    = [this] { proc.clearSnapshot (proc.getSnapA()); grid.repaint(); };
    identityBtn.onClick = [this] { proc.identitySnapshot (proc.getSnapA()); grid.repaint(); };
    loadSnapBtn.onClick = [this] { chooseFile (false, false); };
    saveSnapBtn.onClick = [this] { chooseFile (true, false); };
    loadBankBtn.onClick = [this] { chooseFile (false, true); };
    saveBankBtn.onClick = [this] { chooseFile (true, true); };

    status.setColour (juce::Label::textColourId, textCol.withAlpha (0.6f));
    status.setFont (juce::FontOptions (12.0f));
    status.setText (T ("Clic : on/off \xc2\xb7 Glisser vertical : gain (Maj = fin) \xc2\xb7 Double-clic : 0 dB \xc2\xb7 Clic droit : valeurs \xc2\xb7 Clic sur un en-t\xc3\xaate : diviser / m\xc3\xa9langer"),
                    juce::dontSendNotification);

    setResizable (true, true);
    setResizeLimits (640, 460, 2000, 1600);
    setSize (960, 680);
    timerCallback();
    startTimerHz (30);
}

CrossMatrixEditor::~CrossMatrixEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CrossMatrixEditor::refreshSnapshotNames()
{
    for (int s = 0; s < numSnapshots; ++s)
    {
        const auto txt = juce::String (s + 1) + " - " + proc.getSnapshotName (s);
        snapBox.changeItemText (s + 1, txt);
        targetBox.changeItemText (s + 1, txt);
    }
}

void CrossMatrixEditor::chooseFile (bool save, bool bank)
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Cross Matrix Presets");
    dir.createDirectory();

    const auto ext = bank ? juce::String ("xmbank") : juce::String ("xmsnap");
    const auto title = save ? (bank ? T ("Enregistrer la banque") : T ("Enregistrer le snapshot"))
                            : (bank ? T ("Ouvrir une banque") : T ("Ouvrir un snapshot"));
    auto initial = save ? dir.getChildFile (juce::File::createLegalFileName (bank ? "Banque" : proc.getSnapshotName (proc.getSnapA())) + "." + ext)
                        : dir;

    chooser = std::make_unique<juce::FileChooser> (title, initial, "*." + ext);
    const int flags = save ? (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting)
                           : juce::FileBrowserComponent::openMode;

    juce::Component::SafePointer<CrossMatrixEditor> safe (this);
    chooser->launchAsync (flags | juce::FileBrowserComponent::canSelectFiles, [safe, save, bank, ext] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        auto f = fc.getResult();
        if (f == juce::File()) return;
        auto& pr = safe->proc;
        const int snap = pr.getSnapA();
        bool ok;

        if (save)
        {
            f = f.withFileExtension (ext);
            ok = bank ? pr.saveBankFile (f) : pr.saveSnapshotFile (snap, f);
        }
        else
        {
            ok = bank ? pr.loadBankFile (f) : pr.loadSnapshotFile (snap, f);
        }

        safe->status.setText ((ok ? T ("OK : ") : T ("\xc3\x89" "chec : ")) + f.getFileName(), juce::dontSendNotification);
        safe->refreshSnapshotNames();
        safe->lastSnap = -1;
        safe->grid.repaint();
    });
}

void CrossMatrixEditor::timerCallback()
{
    const int snap = proc.getSnapA();
    const int stamp = proc.uiStamp.load();

    if (snap != lastSnap || stamp != lastStamp)
    {
        lastSnap = snap;
        lastStamp = stamp;
        nameEditor.setText (proc.getSnapshotName (snap), false);
        refreshSnapshotNames();
    }

    const int nIn = proc.getNumIns(), nOut = proc.getNumOuts();
    if (nIn != lastIns || nOut != lastOuts)
    {
        lastIns = nIn;
        lastOuts = nOut;
    }

    for (int c = 0; c < maxChannels; ++c)
    {
        grid.inLevels[(size_t) c]  = juce::jmax (proc.popInputPeak (c),  grid.inLevels[(size_t) c]  * 0.85f);
        grid.outLevels[(size_t) c] = juce::jmax (proc.popOutputPeak (c), grid.outLevels[(size_t) c] * 0.85f);
    }
    grid.repaint();
}

void CrossMatrixEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (panel);
    g.fillRect (getLocalBounds().removeFromTop (124));
    g.setColour (textCol);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText ("CROSS MATRIX", getLocalBounds().removeFromTop (124).removeFromRight (160).reduced (12, 0).removeFromTop (34),
                juce::Justification::centredRight);
}

void CrossMatrixEditor::resized()
{
    auto r = getLocalBounds().reduced (10, 8);

    auto row1 = r.removeFromTop (28);
    row1.removeFromRight (150);
    snapLabel.setBounds (row1.removeFromLeft (64));
    snapBox.setBounds (row1.removeFromLeft (190));
    row1.removeFromLeft (6);
    nameEditor.setBounds (row1.removeFromLeft (juce::jmax (100, row1.getWidth() - 4 * 78)));
    for (auto* b : { &copyBtn, &pasteBtn, &clearBtn, &identityBtn })
    {
        row1.removeFromLeft (4);
        b->setBounds (row1.removeFromLeft (74));
    }

    r.removeFromTop (6);
    auto row2 = r.removeFromTop (26);
    const int bw = juce::jmin (180, row2.getWidth() / 4 - 4);
    for (auto* b : { &loadSnapBtn, &saveSnapBtn, &loadBankBtn, &saveBankBtn })
    {
        b->setBounds (row2.removeFromLeft (bw));
        row2.removeFromLeft (6);
    }

    r.removeFromTop (8);
    auto row3 = r.removeFromTop (30);
    targetLabel.setBounds (row3.removeFromLeft (76));
    targetBox.setBounds (row3.removeFromLeft (170).reduced (0, 2));
    row3.removeFromLeft (8);
    const int sw = row3.getWidth() / 3;
    auto place = [] (juce::Rectangle<int> a, juce::Label& l, juce::Slider& s)
    {
        l.setBounds (a.removeFromLeft (l.getFont().getStringWidth (l.getText()) + 12));
        s.setBounds (a);
    };
    place (row3.removeFromLeft (sw), morphLabel, morphSlider);
    place (row3.removeFromLeft (sw), transitionLabel, transitionSlider);
    place (row3, masterLabel, masterSlider);

    r.removeFromTop (14);
    status.setBounds (r.removeFromBottom (20));
    grid.setBounds (r);
}

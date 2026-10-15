// ME Pro EQ — ME Audio
#include "HeaderBar.h"
#include "PluginProcessor.h"
#include "Theme.h"

namespace mepro::ui
{

using state::ABComparison;

HeaderBar::HeaderBar (EditorModel& m) : model (m)
{
    auto& p = model.getProcessor();

    undoButton.setTooltip ("Undo the last edit made in this window (Ctrl+Z). Host automation is not recorded.");
    redoButton.setTooltip ("Redo (Ctrl+Shift+Z or Ctrl+Y)");
    undoButton.onClick = [this] { performUndo(); };
    redoButton.onClick = [this] { performRedo(); };

    prevPreset.setTooltip ("Previous preset");
    nextPreset.setTooltip ("Next preset");
    presetName.setTooltip ("Preset browser: factory presets by category, your presets, save / delete");
    saveButton.setTooltip ("Save the current settings as a user preset");
    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    presetName.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { openSaveDialog(); };

    for (auto* b : { &slotA, &slotB })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, theme::accent);
        b->setColour (juce::TextButton::textColourOnId, juce::Colour (0xff15110a));
    }
    slotA.setTooltip ("Settings A (each slot keeps its own undo history)");
    slotB.setTooltip ("Settings B (the first switch starts B as a copy of A)");
    copyButton.setTooltip ("Copy the active slot's settings to the other slot");
    slotA.onClick = [this] { selectSlot (false); };
    slotB.onClick = [this] { selectSlot (true); };
    copyButton.onClick = [this] { copySlot(); };

    autoGainButton.setColour (juce::TextButton::buttonOnColourId, theme::good.darker (0.35f));
    autoGainButton.setTooltip ("Auto gain: offsets the output by the EQ curve's static loudness change (pink-noise weighted, "
                               "20 Hz - 20 kHz) so you compare tone, not level. Dynamic gain changes are not compensated. "
                               "The applied gain is shown on the button.");
    autoGainAtt = std::make_unique<ToggleBinding> (*model.globalParam (params::id::autoGain), autoGainButton, model.getUndo());

    bypassButton.setClickingTogglesState (true);
    bypassButton.setColour (juce::TextButton::buttonOnColourId, theme::danger);
    bypassButton.setTooltip ("Global bypass (host-automatable). Bypassed output is the unprocessed input.");
    bypassAtt = std::make_unique<juce::ButtonParameterAttachment> (*model.globalParam (params::id::bypass), bypassButton);

    monoButton.setClickingTogglesState (true);
    monoButton.setTooltip ("Mono compatibility check: listen to (L+R)/2. Monitoring only - never saved or rendered.");
    monoButton.setToggleState (p.getMonoAudition(), juce::dontSendNotification);
    monoButton.onClick = [this] { model.getProcessor().setMonoAudition (monoButton.getToggleState()); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &undoButton, &redoButton, &prevPreset, &nextPreset, &presetName, &saveButton, &slotA, &slotB, &copyButton,
             &autoGainButton, &bypassButton, &monoButton })
        addAndMakeVisible (c);
    refresh();
}

HeaderBar::~HeaderBar()
{
    if (saveDialog != nullptr)
        saveDialog->exitModalState (0);
    saveDialog.reset();
}

bool HeaderBar::performUndo()
{
    const bool ok = model.getUndo().undo();
    refresh();
    return ok;
}

bool HeaderBar::performRedo()
{
    const bool ok = model.getUndo().redo();
    refresh();
    return ok;
}

void HeaderBar::stepPreset (int direction)
{
    model.getProcessor().getPresets().loadNext (direction);
    refresh();
}

void HeaderBar::selectSlot (bool b)
{
    model.getProcessor().getAB().switchTo (b ? ABComparison::Slot::B : ABComparison::Slot::A);
    refresh();
}

void HeaderBar::copySlot()
{
    model.getProcessor().getAB().copyActiveToInactive();
    refresh();
}

void HeaderBar::showPresetMenu()
{
    auto& pm = model.getProcessor().getPresets();
    pm.rescan(); // pick up presets added outside the plugin
    const auto& list = pm.getPresets();

    juce::PopupMenu menu, user;
    juce::String category;
    juce::PopupMenu sub;
    auto flush = [&]
    {
        if (category.isNotEmpty())
            menu.addSubMenu (category, sub);
        sub = {};
    };
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto& info = list[(size_t) i];
        const bool current = info.name == pm.getCurrentName();
        if (! info.factory)
        {
            user.addItem (1000 + i, info.name, true, current);
            continue;
        }
        if (info.category != category)
        {
            flush();
            category = info.category;
        }
        sub.addItem (1000 + i, info.name, true, current);
    }
    flush();
    menu.addSeparator();
    if (user.getNumItems() > 0)
        menu.addSubMenu ("User", user);
    else
        menu.addItem (-1, "User (none saved yet)", false);
    menu.addSeparator();
    menu.addItem (1, "Default (flat, all bands off)");
    menu.addItem (2, "Save as user preset...");
    const int cur = pm.indexOfCurrent();
    const bool curIsUser = cur >= 0 && ! list[(size_t) cur].factory;
    menu.addItem (3, "Delete user preset \"" + pm.getCurrentName() + "\"", curIsUser);
    menu.addItem (4, "Show user preset folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [sp = juce::Component::SafePointer<HeaderBar> (this)] (int r)
                        {
                            if (sp == nullptr || r == 0)
                                return;
                            auto& presets = sp->model.getProcessor().getPresets();
                            const auto& all = presets.getPresets();
                            if (r >= 1000 && r - 1000 < (int) all.size())
                                presets.load (all[(size_t) (r - 1000)]);
                            else if (r == 1)
                                presets.loadDefault();
                            else if (r == 2)
                                sp->openSaveDialog();
                            else if (r == 3)
                            {
                                const int i = presets.indexOfCurrent();
                                if (i >= 0)
                                {
                                    const auto info = all[(size_t) i];
                                    presets.deleteUserPreset (info);
                                }
                            }
                            else if (r == 4)
                            {
                                auto dir = mepro::state::PresetManager::getUserPresetDirectory();
                                dir.createDirectory();
                                dir.revealToUser();
                            }
                            sp->refresh();
                        });
}

void HeaderBar::openSaveDialog()
{
    auto& pm = model.getProcessor().getPresets();
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name for the new user preset:", juce::MessageBoxIconType::NoIcon, this);
    const auto current = pm.getCurrentName();
    const int idx = pm.indexOfCurrent();
    const bool factory = idx >= 0 && pm.getPresets()[(size_t) idx].factory;
    saveDialog->addTextEditor ("name", factory || current == "Default" ? juce::String() : current);
    saveDialog->addTextEditor ("description", {}, "Description (optional)");
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([sp = juce::Component::SafePointer<HeaderBar> (this)] (int result)
    {
        if (sp == nullptr || sp->saveDialog == nullptr)
            return;
        const auto name = sp->saveDialog->getTextEditorContents ("name");
        const auto desc = sp->saveDialog->getTextEditorContents ("description");
        sp->saveDialog.reset();
        if (result != 1)
            return;
        juce::String error;
        if (! sp->model.getProcessor().getPresets().saveUserPreset (name, desc, error))
            juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                              .withIconType (juce::MessageBoxIconType::WarningIcon)
                                              .withTitle ("Preset not saved")
                                              .withMessage (error)
                                              .withButton ("OK")
                                              .withAssociatedComponent (sp.getComponent()),
                                          [] (int) {});
        sp->refresh();
    }), false);
}

void HeaderBar::updatePresetLabel()
{
    auto& pm = model.getProcessor().getPresets();
    auto txt = pm.getCurrentName();
    if (pm.isModified())
        txt << " *";
    if (txt != lastPresetText)
    {
        lastPresetText = txt;
        presetName.setButtonText (txt);
        presetName.setTooltip (pm.isModified() ? "Preset browser - the loaded preset has been changed (*)"
                                               : "Preset browser: factory presets by category, your presets, save / delete");
    }
}

void HeaderBar::refresh()
{
    auto& p = model.getProcessor();
    auto& u = p.getUndoHistory();
    undoButton.setEnabled (u.canUndo());
    redoButton.setEnabled (u.canRedo());

    updatePresetLabel();

    auto& ab = p.getAB();
    const bool isB = ab.getActive() == ABComparison::Slot::B;
    slotA.setToggleState (! isB, juce::dontSendNotification);
    slotB.setToggleState (isB, juce::dontSendNotification);
    copyButton.setButtonText (isB ? "B > A" : "A > B");
    copyButton.setEnabled (ab.slotsDiffer());

    if (autoGainButton.getToggleState())
    {
        const float g = p.getEngine().getAutoGainDb();
        autoGainButton.setButtonText ("AUTO " + juce::String (g >= 0.0f ? "+" : "") + juce::String (g, 1) + " dB");
    }
    else
    {
        autoGainButton.setButtonText ("AUTO GAIN");
    }
    monoButton.setToggleState (p.getMonoAudition(), juce::dontSendNotification);
}

void HeaderBar::paint (juce::Graphics& g)
{
    using namespace theme;
    g.fillAll (panel);
    g.setColour (panelEdge);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

    // ME Audio mark: amber rounded square with "ME", then the product name.
    auto r = getLocalBounds().reduced (12, 9).toFloat();
    auto mark = r.removeFromLeft (r.getHeight()).reduced (1.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (mark, 5.0f);
    g.setColour (juce::Colour (0xff15110a));
    g.setFont (font (mark.getHeight() * 0.52f, true));
    g.drawText ("ME", mark, juce::Justification::centred);
    r.removeFromLeft (10.0f);
    g.setColour (text);
    g.setFont (font (19.0f, true));
    g.drawText ("PRO EQ", r.removeFromLeft (84.0f), juce::Justification::centredLeft);
    g.setColour (textFaint);
    g.setFont (font (10.5f, true));
    g.drawText ("ME AUDIO", r.removeFromLeft (64.0f), juce::Justification::centredLeft);
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    r.removeFromLeft (196);

    // Right group (fixed): AUTO GAIN, MONO, BYPASS
    bypassButton.setBounds (r.removeFromRight (74));
    r.removeFromRight (6);
    monoButton.setBounds (r.removeFromRight (56));
    r.removeFromRight (6);
    autoGainButton.setBounds (r.removeFromRight (112));
    r.removeFromRight (14);

    // Left group: UNDO / REDO, then the preset browser, then A / B / copy.
    undoButton.setBounds (r.removeFromLeft (50));
    r.removeFromLeft (4);
    redoButton.setBounds (r.removeFromLeft (50));
    r.removeFromLeft (14);

    auto abArea = r.removeFromRight (124);
    slotA.setBounds (abArea.removeFromLeft (30));
    abArea.removeFromLeft (2);
    slotB.setBounds (abArea.removeFromLeft (30));
    abArea.removeFromLeft (6);
    copyButton.setBounds (abArea);
    r.removeFromRight (14);

    prevPreset.setBounds (r.removeFromLeft (24));
    r.removeFromLeft (2);
    saveButton.setBounds (r.removeFromRight (50));
    r.removeFromRight (4);
    nextPreset.setBounds (r.removeFromRight (24));
    r.removeFromRight (2);
    presetName.setBounds (r);
}

} // namespace mepro::ui

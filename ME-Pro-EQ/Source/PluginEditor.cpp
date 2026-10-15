// ME Pro EQ — ME Audio
#include "PluginEditor.h"
#include "UI/Theme.h"

MEProEQAudioProcessorEditor::MEProEQAudioProcessorEditor (MEProEQAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      model (p),
      header (model),
      graph (model),
      bandPanel (model),
      meters (model)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (header);
    addAndMakeVisible (graph);
    addAndMakeVisible (bandPanel);
    addAndMakeVisible (meters);

    setResizable (true, true);
    setResizeLimits (kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
    auto ui = processor.getUiState();
    const int w = juce::jlimit (kMinWidth, kMaxWidth, (int) ui.getProperty ("width", kDefaultWidth));
    const int h = juce::jlimit (kMinHeight, kMaxHeight, (int) ui.getProperty ("height", kDefaultHeight));
    setSize (w, h);

    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

MEProEQAudioProcessorEditor::~MEProEQAudioProcessorEditor()
{
    stopTimer();
    processor.setAuditionBand (-1); // auditioning ends when the editor closes
    setLookAndFeel (nullptr);
}

void MEProEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (mepro::ui::theme::background);
}

void MEProEQAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (48));
    bandPanel.setBounds (r.removeFromBottom (juce::jlimit (176, 230, getHeight() / 4)));
    meters.setBounds (r.removeFromRight (104));
    graph.setBounds (r);

    auto ui = processor.getUiState();
    ui.setProperty ("width", getWidth(), nullptr);
    ui.setProperty ("height", getHeight(), nullptr);
}

void MEProEQAudioProcessorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (0.001, 0.25, (now - lastTick) * 0.001);
    lastTick = now;
    refreshNow (dt);
}

void MEProEQAudioProcessorEditor::refreshNow (double dt)
{
    graph.refresh();
    bandPanel.refresh();
    meters.refresh (dt);
    header.refresh();
}

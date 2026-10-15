// ME Pro EQ — ME Audio
#include "ValueKnob.h"
#include "Theme.h"

namespace mepro::ui
{

namespace
{
    constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.25f;
    constexpr float kEndAngle = juce::MathConstants<float>::pi * 2.75f;
    constexpr float kPixelsForFullRange = 220.0f;
} // namespace

ValueKnob::ValueKnob (juce::String labelText) : label (std::move (labelText))
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTitle (label);
}

ValueKnob::~ValueKnob() = default;

void ValueKnob::attach (juce::RangedAudioParameter* p)
{
    if (editor != nullptr)
        editor.reset();
    attachment.reset();
    param = p;
    if (param != nullptr)
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
        {
            value = v;
            repaint();
            if (auto* h = getAccessibilityHandler())
                h->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
        });
        attachment->sendInitialUpdate();
        setHelpText (param->getName (64));
    }
    repaint();
}

juce::Rectangle<float> ValueKnob::knobArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (14.0f);
    r.removeFromBottom (16.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    return r.withSizeKeepingCentre (d, d).reduced (2.0f);
}

void ValueKnob::paint (juce::Graphics& g)
{
    using namespace theme;
    const bool enabled = isEnabled() && param != nullptr;
    auto bounds = getLocalBounds().toFloat();

    g.setColour (enabled ? textDim : textFaint);
    g.setFont (font (11.0f, true));
    g.drawText (label.toUpperCase(), bounds.removeFromTop (14.0f), juce::Justification::centred);

    const auto k = knobArea();
    const float norm = param != nullptr ? param->convertTo0to1 (value) : 0.0f;
    const float radius = k.getWidth() * 0.5f;
    const auto c = k.getCentre();
    const float trackW = juce::jmax (2.5f, radius * 0.16f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - trackW, radius - trackW, 0.0f, kStartAngle, kEndAngle, true);
    g.setColour (panelEdge);
    g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (enabled)
    {
        const float zeroNorm = bipolar ? param->convertTo0to1 (0.0f) : 0.0f;
        const float a0 = kStartAngle + zeroNorm * (kEndAngle - kStartAngle);
        const float a1 = kStartAngle + norm * (kEndAngle - kStartAngle);
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, radius - trackW, radius - trackW, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float inner = radius - trackW * 2.2f;
    g.setColour (enabled ? panelRaised.brighter (0.08f) : panelRaised);
    g.fillEllipse (k.withSizeKeepingCentre (inner * 2.0f, inner * 2.0f));
    const float ang = kStartAngle + norm * (kEndAngle - kStartAngle);
    const juce::Point<float> tip (c.x + std::sin (ang) * inner * 0.82f, c.y - std::cos (ang) * inner * 0.82f);
    g.setColour (enabled ? text : textFaint);
    g.drawLine ({ c.getPointOnCircumference (inner * 0.25f, ang), tip }, juce::jmax (1.5f, radius * 0.07f));

    if (hasKeyboardFocus (false))
    {
        g.setColour (accent.withAlpha (0.6f));
        g.drawEllipse (k.reduced (-1.0f), 1.0f);
    }

    if (editor == nullptr)
    {
        g.setColour (enabled ? text : textFaint);
        g.setFont (font (12.5f));
        const auto txt = param != nullptr ? param->getText (param->convertTo0to1 (value), 32) : juce::String ("-");
        g.drawText (enabled ? txt : juce::String ("-"), getLocalBounds().removeFromBottom (16), juce::Justification::centred);
    }
}

void ValueKnob::resized()
{
    if (editor != nullptr)
        editor->setBounds (getLocalBounds().removeFromBottom (18).reduced (2, 0));
}

void ValueKnob::setNormalised (float norm, bool asGesture)
{
    if (param == nullptr || attachment == nullptr)
        return;
    const float v = param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, norm));
    if (asGesture)
        attachment->setValueAsCompleteGesture (v);
    else
        attachment->setValueAsPartOfGesture (v);
}

void ValueKnob::mouseDown (const juce::MouseEvent& e)
{
    if (param == nullptr || ! isEnabled())
        return;
    grabKeyboardFocus();
    if (e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isAltDown())
    {
        setNormalised (param->getDefaultValue(), true);
        return;
    }
    dragging = true;
    dragFine = e.mods.isShiftDown();
    dragStart = e.position;
    dragStartNorm = param->convertTo0to1 (value);
    attachment->beginGesture();
}

void ValueKnob::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;
    if (e.mods.isShiftDown() != dragFine)
    {
        // Re-anchor when the fine modifier changes so the value never jumps.
        dragFine = e.mods.isShiftDown();
        dragStart = e.position;
        dragStartNorm = param->convertTo0to1 (value);
        return;
    }
    const float dx = e.position.x - dragStart.x, dy = dragStart.y - e.position.y;
    const float pixels = (std::abs (dy) >= std::abs (dx) ? dy : dx);
    const float norm = dragStartNorm + pixels * (dragFine ? 0.1f : 1.0f) / kPixelsForFullRange;
    setNormalised (norm, false);
    if (norm < 0.0f || norm > 1.0f)
    {
        // Re-anchor at the range ends so reversing direction responds immediately.
        dragStart = e.position;
        dragStartNorm = juce::jlimit (0.0f, 1.0f, norm);
    }
}

void ValueKnob::mouseUp (const juce::MouseEvent&)
{
    if (dragging && attachment != nullptr)
        attachment->endGesture();
    dragging = false;
}

void ValueKnob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (param != nullptr && isEnabled())
        showEditor();
}

void ValueKnob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (param == nullptr || ! isEnabled())
        return;
    const float step = (e.mods.isShiftDown() ? 0.002f : 0.02f) * (w.deltaY >= 0 ? 1.0f : -1.0f) * (w.isReversed ? -1.0f : 1.0f);
    setNormalised (param->convertTo0to1 (value) + step, true);
}

bool ValueKnob::keyPressed (const juce::KeyPress& k)
{
    if (param == nullptr || ! isEnabled())
        return false;
    const float step = k.getModifiers().isShiftDown() ? 0.002f : 0.02f;
    const float n = param->convertTo0to1 (value);
    if (k == juce::KeyPress::upKey || k == juce::KeyPress::rightKey)
        setNormalised (n + step, true);
    else if (k == juce::KeyPress::downKey || k == juce::KeyPress::leftKey)
        setNormalised (n - step, true);
    else if (k == juce::KeyPress::returnKey)
        showEditor();
    else
        return false;
    return true;
}

void ValueKnob::showEditor()
{
    editor = std::make_unique<juce::TextEditor>();
    editor->setFont (theme::font (12.5f));
    editor->setJustification (juce::Justification::centred);
    editor->setText (param->getText (param->convertTo0to1 (value), 32), false);
    editor->selectAll();
    editor->onReturnKey = [this] { commitEditor(); };
    editor->onEscapeKey = [this] { juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<ValueKnob> (this)] { if (sp) { sp->editor.reset(); sp->repaint(); } }); };
    editor->onFocusLost = [this] { commitEditor(); };
    addAndMakeVisible (*editor);
    resized();
    editor->grabKeyboardFocus();
    repaint();
}

void ValueKnob::commitEditor()
{
    if (editor == nullptr || param == nullptr)
        return;
    const auto text = editor->getText();
    // The parameter's own text-to-value function understands units ("1.2k", "-3 dB", "40 ms").
    const float norm = param->getValueForText (text);
    juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<ValueKnob> (this)] { if (sp) { sp->editor.reset(); sp->repaint(); } });
    if (text.trim().isNotEmpty())
        setNormalised (norm, true);
}

std::unique_ptr<juce::AccessibilityHandler> ValueKnob::createAccessibilityHandler()
{
    struct ValueInterface final : public juce::AccessibilityRangedNumericValueInterface
    {
        explicit ValueInterface (ValueKnob& k) : knob (k) {}
        bool isReadOnly() const override { return knob.param == nullptr; }
        double getCurrentValue() const override { return knob.param != nullptr ? knob.param->convertTo0to1 (knob.value) : 0.0; }
        void setValue (double v) override { knob.setNormalised ((float) v, true); }
        AccessibleValueRange getRange() const override { return { { 0.0, 1.0 }, 0.01 }; }
        ValueKnob& knob;
    };
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::slider,
                                                         juce::AccessibilityActions(),
                                                         juce::AccessibilityHandler::Interfaces { std::make_unique<ValueInterface> (*this) });
}

} // namespace mepro::ui

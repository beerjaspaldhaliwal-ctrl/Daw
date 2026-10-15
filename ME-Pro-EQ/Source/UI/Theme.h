// ME Pro EQ — ME Audio
// Visual identity: colours, fonts and band palette (original ME Audio design).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace mepro::ui::theme
{

inline const juce::Colour background { 0xff0a0d12 };
inline const juce::Colour panel { 0xff121720 };
inline const juce::Colour panelRaised { 0xff19202b };
inline const juce::Colour panelEdge { 0xff252e3b };
inline const juce::Colour graphTop { 0xff10151d };
inline const juce::Colour graphBottom { 0xff0b0f15 };
inline const juce::Colour gridMinor { 0x0dffffff };
inline const juce::Colour gridMajor { 0x1affffff };
inline const juce::Colour gridZero { 0x33ffffff };
inline const juce::Colour text { 0xffdfe5ee };
inline const juce::Colour textDim { 0xff8893a5 };
inline const juce::Colour textFaint { 0xff586273 };
inline const juce::Colour accent { 0xfff2b544 };      // ME Audio amber
inline const juce::Colour accentSoft { 0x40f2b544 };
inline const juce::Colour curve { 0xffeef3fa };
inline const juce::Colour danger { 0xffe5534b };
inline const juce::Colour good { 0xff57c785 };
inline const juce::Colour preSpectrum { 0xff4f6b8a };
inline const juce::Colour postSpectrum { 0xff8fb8e8 };

// Channel-domain curve colours (used when bands route to L/R/M/S)
inline const juce::Colour leftCurve { 0xff6fc3ff };
inline const juce::Colour rightCurve { 0xffff8f7a };
inline const juce::Colour midCurve { 0xffa6e07a };
inline const juce::Colour sideCurve { 0xffd29bff };

// 16 distinct but restrained band colours: golden-ratio hue spacing, moderate saturation.
inline juce::Colour bandColour (int band)
{
    const float h = (float) std::fmod (0.07 + band * 0.6180339887, 1.0);
    return juce::Colour::fromHSV (h, 0.55f, 0.97f, 1.0f);
}

inline juce::Font font (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::Font monoFont (float height)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
}

} // namespace mepro::ui::theme

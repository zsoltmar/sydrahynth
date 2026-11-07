#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "BinaryData.h"

// The palette and widget styling of design/sydrahynth.svg: a near-black, warm
// theme with red accents. Shared by the panel, the status bar and the patch
// browser.
namespace theme {

inline const juce::Colour kBackground{0xff1f1d1d};   // window and panel
inline const juce::Colour kCard{0xff1e1b1b};         // section card fill
inline const juce::Colour kBrowserCard{0xd6181617};  // patch browser card (84%)
inline const juce::Colour kWell{0xff0e0d0d};         // recessed areas (bank bar, list)
inline const juce::Colour kWellDeep{0xff0d0c0d};
inline const juce::Colour kText{0xffe8e8e8};
inline const juce::Colour kTextSoft{0xffc4c4c4};
inline const juce::Colour kTextDim{0xff9e9e9e};
inline const juce::Colour kTextFaint{0xff858585};
inline const juce::Colour kAccent{0xffff3b3b};
inline const juce::Colour kAccentHot{0xffff5a5a};    // arc core, knob tick
inline const juce::Colour kAccentBright{0xffff6b6b}; // dot centres
inline const juce::Colour kAccentDim{0xffb24f4f};    // secondary legends
inline const juce::Colour kLedOff{0xff4a4d52};       // unlit button LED bar
inline const juce::Colour kRingOff{0xff4a4d52};      // unlit encoder ring dot
inline const juce::Colour kTrack{0xff54575d};        // knob value-arc track
inline const juce::Colour kTick{0xff16171a};         // knob tick body
inline const juce::Colour kLine{0xff6e6e6e};         // module signal lines

// Clear Sans, embedded with the plugin: the design's control font.
inline const juce::Typeface::Ptr& clearSans(int weight) {
    static const juce::Typeface::Ptr regular =
        juce::Typeface::createSystemTypefaceFor(BinaryData::ClearSansRegular_ttf, BinaryData::ClearSansRegular_ttfSize);
    static const juce::Typeface::Ptr medium =
        juce::Typeface::createSystemTypefaceFor(BinaryData::ClearSansMedium_ttf, BinaryData::ClearSansMedium_ttfSize);
    static const juce::Typeface::Ptr bold =
        juce::Typeface::createSystemTypefaceFor(BinaryData::ClearSansBold_ttf, BinaryData::ClearSansBold_ttfSize);
    return weight >= 700 ? bold : (weight >= 500 ? medium : regular);
}

inline juce::Font font(float size, float letterSpacing = 0.0f, int weight = 400) {
    juce::Font f(juce::FontOptions(clearSans(weight)).withPointHeight(size));
    f.setExtraKerningFactor(letterSpacing / size);
    return f;
}

inline float textWidth(const juce::Font& f, juce::StringRef text) {
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(f, text, 0.0f, 0.0f);
    return glyphs.getBoundingBox(0, -1, true).getWidth();
}

// A panel button: a flat face with a hairline edge; pressed goes darker.
inline void paintButtonFace(juce::Graphics& g, juce::Rectangle<float> r, bool held, float corner = 6.0f) {
    g.setColour(held ? juce::Colour(0xff1d1e20) : juce::Colour(0xff2e2f32));
    g.fillRoundedRectangle(r, corner);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRoundedRectangle(r.reduced(0.5f), corner - 0.5f, 1.0f);
}

// The LED inside a panel button: a flat dark slot holding the red bar. It sits
// at 77% of the face height, ~half the face width.
inline void paintLedBar(juce::Graphics& g, juce::Rectangle<float> button, float level) {
    const juce::Rectangle<float> slot = button.withSizeKeepingCentre(juce::jmin(button.getWidth() * 0.48f, 34.0f), 3.0f)
                                            .translated(0, button.getHeight() * 0.265f);
    g.setColour(juce::Colour(0xff141414));
    g.fillRoundedRectangle(slot, slot.getHeight() / 2);
    // The unlit LED still reads as a dim red bar inside the slot.
    const juce::Rectangle<float> bar = slot.reduced(1.0f, 0.0f);
    g.setColour(level <= 0.01f ? kAccent.withAlpha(0.25f) : kAccent.withAlpha(0.35f + 0.65f * level));
    g.fillRoundedRectangle(bar, bar.getHeight() / 2);
}

// A section card: its fill blends into the background; the border is an
// engraved bezel — a dark line half a px up-left of the edge and a light one
// half a px down-right, so top/left read dark and bottom/right read light
// with no line floating outside the card.
inline void paintCard(juce::Graphics& g, juce::Rectangle<float> r, float corner = 13.0f,
                      juce::Colour fill = kBackground) {
    g.setColour(fill);
    g.fillRoundedRectangle(r, corner);
    const juce::Rectangle<float> edge = r.reduced(0.5f);
    // The design's bezel: a faint white hairline 2px down-right beneath the
    // black edge — it whispers below/right rather than reading as a border.
    g.setColour(juce::Colours::white.withAlpha(0.045f));
    g.drawRoundedRectangle(edge.translated(2.0f, 2.0f), corner, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.drawRoundedRectangle(edge, corner, 1.0f);
}

// The section name on the card's top edge, the border knocked out behind it.
inline void paintCardHeading(juce::Graphics& g, juce::Rectangle<float> card, juce::StringRef text, juce::Colour background) {
    const juce::Font f = font(13.0f, 2.4f);
    const float width = textWidth(f, text) + 18.0f;
    juce::Rectangle<float> r(width, 16.0f);
    r.setCentre(card.getCentreX(), card.getY());
    g.setColour(background);
    g.fillRect(r);
    g.setColour(kText);
    g.setFont(f);
    g.drawText(text, r, juce::Justification::centred);
}

// A knob body: a flat dark bezel with a flat lighter cap — two tones, no
// shading.
inline void paintKnobBody(juce::Graphics& g, juce::Point<float> c, float radius, float capRatio = 0.8f) {
    g.setColour(juce::Colour(0xff242629));
    g.fillEllipse(juce::Rectangle<float>(radius * 2, radius * 2).withCentre(c));
    g.setColour(juce::Colour(0xff3d4046));
    g.fillEllipse(juce::Rectangle<float>(radius * capRatio * 2, radius * capRatio * 2).withCentre(c));
}

// The red pointer on the knob cap: a single flat bar.
inline void paintKnobTick(juce::Graphics& g, juce::Point<float> c, float radius, float angle, float capRatio = 0.8f) {
    const float cap = radius * capRatio;
    juce::Path tick;
    tick.startNewSubPath(c.getPointOnCircumference(cap * 0.22f, angle));
    tick.lineTo(c.getPointOnCircumference(cap * 0.92f, angle));
    g.setColour(kAccent);
    g.strokePath(tick, juce::PathStrokeType(juce::jmax(3.5f, radius * 0.16f), juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
}

// The value arc around a knob: a dim track covering only the wiper's 270-degree
// travel — the gap sits at the bottom, above the label — with a flat lit arc
// up to the value. angle = pi*(-0.75+1.5*v); min at 7:30, max at 4:30 (JUCE
// angles run clockwise from 12 o'clock).
inline void paintValueArc(juce::Graphics& g, juce::Point<float> c, float radius, float value) {
    const float start = juce::MathConstants<float>::pi * -0.75f;
    juce::Path track;
    track.addCentredArc(c.x, c.y, radius, radius, 0.0f, start,
                        start + juce::MathConstants<float>::pi * 1.5f, true);
    g.setColour(juce::Colour(0xff35383d));
    g.strokePath(track, juce::PathStrokeType(juce::jmax(1.6f, radius * 0.05f), juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
    const float end = start + juce::MathConstants<float>::pi * 1.5f * value;
    if (value > 0.005f) {
        juce::Path lit;
        lit.addCentredArc(c.x, c.y, radius, radius, 0.0f, start, end, true);
        g.setColour(kAccent);
        g.strokePath(lit, juce::PathStrokeType(juce::jmax(2.0f, radius * 0.065f), juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
}

// One encoder ring LED: a flat dot, red when lit.
inline void paintRingDot(juce::Graphics& g, juce::Point<float> c, float radius, bool lit) {
    g.setColour(lit ? kAccent : kRingOff);
    g.fillEllipse(juce::Rectangle<float>(radius * 2, radius * 2).withCentre(c));
}

// The shared button/combobox LookAndFeel: dark gradient pills with hairline
// light edges, letter-spaced caps labels.
class LookAndFeel : public juce::LookAndFeel_V4 {
public:
    LookAndFeel() {
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff141313));
        setColour(juce::ComboBox::textColourId, kText);
        setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::ComboBox::arrowColourId, kTextDim);
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff141313));
        setColour(juce::PopupMenu::textColourId, kText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha(0.55f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        setColour(juce::TextButton::textColourOffId, kText);
        setColour(juce::TextButton::textColourOnId, kText);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down) override {
        const juce::Rectangle<float> r = b.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
        paintButtonFace(g, r, down, r.getHeight() * 0.28f);
        if (highlighted) {
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.fillRoundedRectangle(r, r.getHeight() * 0.28f);
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override {
        g.setColour(kText);
        g.setFont(font(juce::jmin(12.0f, b.getHeight() * 0.4f), 0.9f));
        g.drawFittedText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 2);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override {
        juce::Rectangle<float> r(0.5f, 0.5f, width - 1.0f, height - 1.0f);
        g.setColour(juce::Colour(0xff141313));
        g.fillRoundedRectangle(r, r.getHeight() / 2);
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawRoundedRectangle(r, r.getHeight() / 2, 1.0f);
        const float cx = width - height * 0.8f, cy = height * 0.5f;
        juce::Path arrow;
        arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
        g.setColour(kTextDim);
        g.fillPath(arrow);
        if (!box.isEnabled())
            g.setOpacity(0.5f);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(13.5f, 0.9f); }
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override {
        label.setBounds(1, 1, box.getWidth() - box.getHeight() - 4, box.getHeight() - 2);
        label.setJustificationType(juce::Justification::centred);
    }
};

}  // namespace theme

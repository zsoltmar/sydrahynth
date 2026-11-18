#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <unordered_map>

#include "BinaryData.h"

// The palette and widget styling of design/sydrahynth.svg: a near-black, warm
// theme with red accents. Shared by the panel, the status bar and the patch
// browser.
namespace theme {

inline const juce::Colour kBackground{0xff1f1d1d};   // window and panel
inline const juce::Colour kHeader{0xff141212};       // status bar, a step darker than the panel
inline const juce::Colour kCard{0xff1e1b1b};         // section card fill
inline const juce::Colour kBrowserCard{0xd6181617};  // patch browser card (84%)
inline const juce::Colour kWell{0xff0e0d0d};         // recessed areas (bank bar, list)
inline const juce::Colour kWellDeep{0xff0d0c0d};
inline const juce::Colour kText{0xffe8e8e8};
inline const juce::Colour kTextSoft{0xffc4c4c4};
inline const juce::Colour kTextDim{0xff9e9e9e};
inline const juce::Colour kTextFaint{0xff858585};
inline const juce::Colour kAccent{0xff09fc61};
inline const juce::Colour kAccentHot{0xff40ff7d};    // arc core, knob tick
inline const juce::Colour kAccentBright{0xff66ff94}; // dot centres
inline const juce::Colour kAccentDim{0xff3fa05a};    // secondary legends
inline const juce::Colour kLedOff{0xff4a4d52};       // unlit button LED bar
inline const juce::Colour kRingOff{0xff4a4d52};      // unlit encoder ring dot
inline const juce::Colour kTrack{0xff54575d};        // knob value-arc track
inline const juce::Colour kKnob{0xff48484c};         // knob face
inline const juce::Colour kTick{0xff16171a};         // knob tick body
inline const juce::Colour kLine{0xff6e6e6e};         // module signal lines

// Gabarito Regular — the family's lightest cut (the weight axis starts at 400).
inline const juce::Typeface::Ptr& gabarito() {
    static const juce::Typeface::Ptr regular =
        juce::Typeface::createSystemTypefaceFor(BinaryData::GabaritoRegular_ttf, BinaryData::GabaritoRegular_ttfSize);
    return regular;
}

inline juce::Font font(float size, float letterSpacing = 0.0f) {
    juce::Font f(juce::FontOptions(gabarito()).withPointHeight(size));
    f.setExtraKerningFactor(letterSpacing / size);
    return f;
}

inline float textWidth(const juce::Font& f, juce::StringRef text) {
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(f, text, 0.0f, 0.0f);
    return glyphs.getBoundingBox(0, -1, true).getWidth();
}

// The sliced button artwork: 280x200 frames holding a solid face at (40,50)
// 200x100, with the drop shadow padding below it. The glow layer's LED bar
// sits inside the face's lower half. Both layers share the same frame.
// Margins are derived from the height, matching how assembledSlices draws
// the caps (capW = 70*frameH/200 → margin 40/70 of a cap), so the rendered
// face lands exactly on r regardless of button width.
inline juce::Rectangle<float> buttonFrame(juce::Rectangle<float> r) {
    const float my = r.getHeight() * 0.5f, mx = r.getHeight() * 0.4f;
    return {r.getX() - mx, r.getY() - my, r.getWidth() + 2.0f * mx, r.getHeight() + 2.0f * my};
}

inline const juce::Image& buttonSlice(int i) {
    static const juce::Image imgs[3] = {
        juce::ImageFileFormat::loadFrom(BinaryData::buttonleft_png, BinaryData::buttonleft_pngSize),
        juce::ImageFileFormat::loadFrom(BinaryData::buttonmiddle_png, BinaryData::buttonmiddle_pngSize),
        juce::ImageFileFormat::loadFrom(BinaryData::buttonright_png, BinaryData::buttonright_pngSize)};
    return imgs[i];
}

inline const juce::Image& glowSlice(int i) {
    static const juce::Image imgs[3] = {
        juce::ImageFileFormat::loadFrom(BinaryData::glowleft_png, BinaryData::glowleft_pngSize),
        juce::ImageFileFormat::loadFrom(BinaryData::glowmiddle_png, BinaryData::glowmiddle_pngSize),
        juce::ImageFileFormat::loadFrom(BinaryData::glowright_png, BinaryData::glowright_pngSize)};
    return imgs[i];
}

// Assembles a slice set into one image sized to the target frame: the caps
// keep their aspect (scaled only by the frame's vertical factor) and the
// middle fills the gap, so seams land inside the assembled image rather than
// at a draw boundary. Cached per (w,h) — blitted 1:1 afterwards.
inline const juce::Image& assembledSlices(juce::Rectangle<float> d, bool glow) {
    const int w = juce::roundToInt(d.getWidth()), h = juce::roundToInt(d.getHeight());
    static std::unordered_map<std::uint64_t, juce::Image> cache;
    const std::uint64_t key = (std::uint64_t(w) << 17) | (std::uint64_t(h) << 1) | std::uint64_t(glow);
    juce::Image& img = cache[key];
    if (img.isNull()) {
        img = juce::Image(juce::Image::ARGB, w, h, true);
        juce::Graphics ig(img);
        const int capW = juce::jmin(w / 2,
                                    juce::roundToInt((glow ? 120.0f : 70.0f) * static_cast<float>(h) / 200.0f));
        const juce::Image& l = glow ? glowSlice(0) : buttonSlice(0);
        const juce::Image& m = glow ? glowSlice(1) : buttonSlice(1);
        const juce::Image& r = glow ? glowSlice(2) : buttonSlice(2);
        ig.drawImage(l, 0, 0, capW, h, 0, 0, l.getWidth(), l.getHeight());
        ig.drawImage(m, capW, 0, w - 2 * capW, h, 0, 0, m.getWidth(), m.getHeight());
        ig.drawImage(r, w - capW, 0, capW, h, 0, 0, r.getWidth(), r.getHeight());
    }
    return img;
}

// The pressed face: the assembled artwork with every pixel darkened in
// place, so the whole button — caps, bevel and middle — sinks uniformly.
inline const juce::Image& pressedSlices(juce::Rectangle<float> d) {
    const int w = juce::roundToInt(d.getWidth()), h = juce::roundToInt(d.getHeight());
    static std::unordered_map<std::uint64_t, juce::Image> cache;
    const std::uint64_t key = (std::uint64_t(w) << 17) | (std::uint64_t(h) << 1);
    juce::Image& img = cache[key];
    if (img.isNull()) {
        img = assembledSlices(d, false).createCopy();
        const juce::Image::BitmapData bits(img, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < bits.height; ++y)
            for (int x = 0; x < bits.width; ++x)
                bits.setPixelColour(x, y, bits.getPixelColour(x, y).withMultipliedBrightness(0.55f));
    }
    return img;
}

// A panel button: the sliced face artwork; held draws the darkened copy so
// the whole face reads depressed. clipRadius rounds the face harder than
// the artwork's own edge for buttons that should read as pills. faceOnly
// draws just the face sub-rect of the frame: for components whose Graphics
// is clipped to their bounds, where the baked shadow would otherwise be
// cut hard at the corners.
inline void paintButtonFace(juce::Graphics& g, juce::Rectangle<float> r, bool held, float clipRadius = 0.0f,
                            bool faceOnly = false) {
    const juce::Rectangle<float> d = buttonFrame(r);
    const juce::Image& img = held ? pressedSlices(d) : assembledSlices(d, false);
    g.saveState();
    if (clipRadius > 0.0f) {
        juce::Path clip;
        clip.addRoundedRectangle(r, clipRadius);
        g.reduceClipRegion(clip);
    }
    if (faceOnly) {
        const int mx = juce::roundToInt((img.getWidth() - r.getWidth()) / 2.0f);
        const int my = juce::roundToInt((img.getHeight() - r.getHeight()) / 2.0f);
        g.drawImage(img, juce::roundToInt(r.getX()), juce::roundToInt(r.getY()),
                    juce::roundToInt(r.getWidth()), juce::roundToInt(r.getHeight()),
                    mx, my, juce::roundToInt(r.getWidth()), juce::roundToInt(r.getHeight()));
    } else {
        g.drawImageAt(img, juce::roundToInt(d.getX()), juce::roundToInt(d.getY()));
    }
    g.restoreState();
}

// The LED inside a panel button: the sliced glow layer, dimmed when unlit.
inline void paintLedBar(juce::Graphics& g, juce::Rectangle<float> button, float level) {
    const juce::Rectangle<float> d = buttonFrame(button);
    g.setOpacity(0.12f + 0.88f * level);
    const juce::Image& img = assembledSlices(d, true);
    g.drawImageAt(img, juce::roundToInt(d.getX()), juce::roundToInt(d.getY()));
    g.setOpacity(1.0f);
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
        // Component bounds carry the whole frame (face + shadow margins) so
        // the artwork draws exactly like the painted panel buttons.
        const auto bounds = b.getLocalBounds().toFloat();
        const float faceH = bounds.getHeight() * 0.5f, mx = faceH * 0.4f;
        const juce::Rectangle<float> r = {bounds.getX() + mx, bounds.getY() + bounds.getHeight() * 0.25f,
                                          bounds.getWidth() - 2.0f * mx, faceH};
        paintButtonFace(g, r, down);
        if (highlighted) {
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.fillRoundedRectangle(r, r.getHeight() * 0.14f);
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

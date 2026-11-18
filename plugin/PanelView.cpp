#include "PanelView.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "HydraProcessor.h"
#include "Theme.h"

namespace {
using Input = hydra::Emulator::PanelInput;
constexpr int kShiftButton = 62;
// A button pressed together with SHIFT goes down this much after SHIFT, and
// SHIFT is let go this much after the button: the firmware scans the buttons
// every 5 ms and must see SHIFT held when the other button arrives.
constexpr int kShiftLeadMs = 20, kShiftTrailMs = 80;

// Positions from design/panel-upd.svg. Arp shifts left, filter knobs follow
// part of the way, and the right-hand block (systems, master, displays) drops.
constexpr float kArpX0 = 250.0f, kArpPitch = 167.0f, kArpKnobY = 152.0f, kArpButtonY = 256.0f;
// Four equal columns: cutoff, resonance, drive, then the assignment buttons.
// The button pair is as tall as a knob and centred on the knob.
constexpr float kFilterKnobR = 46.0f;
constexpr float kFilterPitch = 180.25f;
constexpr float kFilterX0 = 222.125f;
constexpr float kFilterKnobY = 471.0f;
constexpr float kFilterButtonW = 100.0f;
constexpr float kFilterButtonX = kFilterX0 + 3.0f * kFilterPitch;
constexpr float kFilterButtonY0 = kFilterKnobY - 26.5f;
constexpr float kFilterButtonY1 = kFilterKnobY + 26.5f;
constexpr float kVolumeX = 62.0f, kVolumeY = 300.0f, kVolumeTravel = 196.0f;

// Buttons: label, keyscan id, LED index (-1: none), centre and size on the canvas.
struct Button {
    const char* label;
    int id, led;
    float x, y, w = 70, h = 36;
};
const Button kButtons[] = {
    // Arpeggiator, one button under each switch.
    {"ON", 26, 30, kArpX0, kArpButtonY, 108, 39},
    {"LATCH", 28, 31, kArpX0 + kArpPitch, kArpButtonY, 108, 39},
    {"TRIPLET", 46, 32, kArpX0 + 2 * kArpPitch, kArpButtonY, 108, 39},
    {"TAP TEMPO", 48, 29, kArpX0 + 3 * kArpPitch, kArpButtonY, 108, 39},
    // Knob assignment for cutoff and resonance
    {"FILTER 1", 79, 26, kFilterButtonX, kFilterButtonY0, kFilterButtonW, 39},
    {"FILTER 2", 80, 27, kFilterButtonX, kFilterButtonY1, kFilterButtonW, 39},
    // Patch and system
    {"BROWSE", 45, 34, 963, 149, 76, 39}, {"SAVE", 50, 33, 963, 234, 76, 39}, {"SYSTEM", 27, 38, 963, 308, 76, 39},
    {"INIT", 44, 35, 963, 373, 76, 39}, {"RANDOM", 59, 36, 963, 448, 76, 39}, {"SHIFT", 62, 39, 963, 542, 76, 39},
    // Navigation
    {"<", 51, -1, 1060, 449, 62, 39}, {">", 55, -1, 1170, 449, 62, 39}, {"HOME", 49, -1, 1115, 542, 172, 39},
    {"EXIT", 7, 45, 1858, 144, 100, 39}, {"VOICE", 56, 46, 1858, 233, 100, 39}, {"UP", 60, 47, 1858, 310, 100, 39},
    {"DOWN", 61, 48, 1858, 375, 100, 39}, {"MACRO ASN", 57, 49, 1858, 447, 100, 39}, {"MOD MATRIX", 58, 58, 1858, 537, 100, 39},
    // Encoder push buttons
    {"", 33, 50, 1356, 237, 66, 32}, {"", 34, 51, 1474, 237, 66, 32}, {"", 35, 52, 1594, 237, 66, 32},
    {"", 36, 53, 1711, 237, 66, 32}, {"", 37, 54, 1355, 451, 66, 32}, {"", 38, 55, 1475, 451, 66, 32},
    {"", 39, 56, 1594, 451, 66, 32}, {"", 40, 57, 1712, 451, 66, 32},
    // Module buttons
    {"OSC 1", 11, 0, 86, 738, 71, 39}, {"MUTANT 1", 14, 3, 191, 738, 93, 39}, {"MUTANT 2", 15, 4, 303, 738, 93, 39},
    {"OSC 2", 12, 1, 86, 809, 71, 39}, {"MUTANT 3", 16, 5, 191, 809, 93, 39}, {"MUTANT 4", 3, 6, 303, 809, 93, 39},
    {"OSC 3", 13, 2, 86, 882, 71, 39}, {"RING-NOISE", 17, 7, 295, 882, 109, 39},
    {"MIXER", 0, 8, 401, 809, 71, 39}, {"FILTER 1", 24, 19, 506, 775, 76, 39}, {"FILTER 2", 25, 20, 506, 845, 76, 39},
    {"AMP", 1, 21, 610, 809, 71, 39}, {"PRE-FX", 2, 22, 713, 809, 83, 39}, {"DELAY", 4, 23, 819, 775, 76, 39},
    {"REVERB", 5, 24, 819, 845, 76, 39}, {"POST-FX", 6, 25, 934, 809, 93, 39},
    {"ENV 1", 70, 9, 506, 694, 71, 39}, {"ENV 2", 71, 10, 610, 694, 71, 39}, {"ENV 3", 72, 11, 714, 694, 71, 39},
    {"ENV 4", 73, 12, 819, 694, 71, 39}, {"ENV 5", 18, 13, 923, 694, 71, 39},
    {"LFO 1", 66, 14, 506, 925, 71, 39}, {"LFO 2", 67, 15, 610, 925, 71, 39}, {"LFO 3", 68, 16, 714, 925, 71, 39},
    {"LFO 4", 69, 17, 819, 925, 71, 39}, {"LFO 5", 47, 18, 923, 925, 71, 39},
};

// Secondary functions printed in orange under some buttons (used with SHIFT).
struct Legend {
    int id;
    const char* text;
};
const Legend kShiftLegends[] = {{26, "EDIT"}, {28, "SUSTAIN"}, {45, "FAVORITE"}, {49, "PANIC"}, {51, "-10"}, {55, "+10"}};

// The pads' SHIFT functions, printed under them: scales on the lower two rows,
// octave, pad mode and key on the top row (pad number = row * 8 + column + 1).
const char* const kPadLegends[3][8] = {
    {"Chromatic", "Major", "Maj Pent", "Min Pent", "Blues", "Bebop", "Whole Tone", "Dim Whole"},
    {"Dorian", "Phrygian", "Lydian", "Mixolydian", "Minor", "Locrian", "Harmonic Min", "Melodic Min"},
    {"OCT -", "OCT +", "PAD MODE -", "PAD MODE +", "KEY", "", "", ""},
};

// Numbered section rules. x is the number's left edge, y its baseline; the
// title starts 26px to the right and the rule sits 12.5px under the baseline.
struct Section {
    const char* number;
    const char* title;
    float x, y, ruleRight;
};
const Section kSections[] = {
    {"01", "ARPEGGIATOR", 136.525f, 45.157f, 852.25f},
    {"02", "FILTER CONTROLS", 136.525f, 371.0f, 852.25f},
    {"03", "MAIN SYSTEMS", 926.1375f, 45.157f, 1210.0f},
    {"04", "MASTER CONTROL", 1284.0f, 45.157f, 1910.2305f},
    {"05", "MODULE SELECT", 48.51f, 631.927f, 993.5f},
    {"06", "KEYBOARD", 1064.4f, 631.927f, 1906.2f},
};

// Knobs: master volume (a vertical fader; x/y is the track centre and radius
// is half its travel), the arpeggiator switches and the analog knobs.
struct Knob {
    enum class Kind { Volume, Arp, Analog } kind;
    const char* label;
    int index;  // switch or knob
    float x, y, radius;
};
const Knob kKnobs[] = {
    {Knob::Kind::Volume, "MASTER VOLUME", 0, kVolumeX, kVolumeY, kVolumeTravel},
    {Knob::Kind::Arp, "MODE", hydra::FrontPanel::kArpMode, kArpX0, kArpKnobY, 28},
    {Knob::Kind::Arp, "OCTAVE", hydra::FrontPanel::kArpOctave, kArpX0 + kArpPitch, kArpKnobY, 28},
    {Knob::Kind::Arp, "DIVISION", hydra::FrontPanel::kArpDivision, kArpX0 + 2 * kArpPitch, kArpKnobY, 28},
    {Knob::Kind::Arp, "SWING", hydra::FrontPanel::kArpSwing, kArpX0 + 3 * kArpPitch, kArpKnobY, 28},
    {Knob::Kind::Analog, "CUTOFF", hydra::FrontPanel::kCutoff, kFilterX0, kFilterKnobY, kFilterKnobR},
    {Knob::Kind::Analog, "RESONANCE", hydra::FrontPanel::kResonance, kFilterX0 + kFilterPitch, kFilterKnobY, kFilterKnobR},
    {Knob::Kind::Analog, "DRIVE / MORPH", hydra::FrontPanel::kDriveMorph, kFilterX0 + 2 * kFilterPitch, kFilterKnobY, kFilterKnobR},
};

constexpr float kEncoderX[4] = {1356, 1474, 1594, 1711}, kEncoderY[2] = {156, 540}, kEncoderRadius = 26;
constexpr float kDataX = 1114, kDataY = 186, kDataRadius = 72;

// Displays: the main one, then the four small ones (top-left corner and size);
// the small ones sit in one continuous black screen.
const juce::Rectangle<float> kDisplays[] = {
    {1020, 290, 190, 105}, {1296, 312, 120, 60}, {1414, 312, 120, 60}, {1534, 312, 120, 60}, {1651, 312, 120, 60}};

// The screen behind the small displays (they share one black area).
constexpr float kSmallDisplayX = 1284, kSmallDisplayY = 290, kSmallDisplayW = 498, kSmallDisplayH = 105;

constexpr float kPadW = 83, kPadH = 64, kPadX = 1064.4f, kPadPitch = 108.4f;
constexpr float kPadY[3] = {902, 803, 704};  // row centres, bottom row first
juce::Rectangle<float> padBounds(int row, int column) {
    return {kPadX + kPadPitch * static_cast<float>(column), kPadY[row] - kPadH / 2, kPadW, kPadH};
}

juce::Rectangle<float> buttonBounds(const Button& b) { return {b.x - b.w / 2, b.y - b.h / 2, b.w, b.h}; }
juce::Point<float> encoderCentre(int e) { return {kEncoderX[e & 3], kEncoderY[e >> 2]}; }

// The master volume knob: a static body PNG plus one of 127 wiper frames
// covering the 270-degree travel. Frames are decoded lazily into a cache.
const juce::Image& knobBodyImage() {
    static const juce::Image img = [] {
        return juce::ImageFileFormat::loadFrom(BinaryData::knobsh_png,
                                               BinaryData::knobsh_pngSize);
    }();
    return img;
}
const juce::Image& knobWiperImage(int frame) {
    static std::array<juce::Image, 127> frames;
    juce::Image& img = frames[static_cast<std::size_t>(frame)];
    if (img.isNull()) {
        char name[24];
        std::snprintf(name, sizeof name, "pip_%03d_png", frame);
        int size = 0;
        if (const char* data = BinaryData::getNamedResource(name, size))
            img = juce::ImageFileFormat::loadFrom(data, static_cast<std::size_t>(size));
    }
    return img;
}

// Draws the knob artwork centred on c so that artRadius (in the 1024-px
// source space; the knob edge is ~333, the dome face ~240) lands on the
// knob's radius. A small artRadius enlarges the art so the face dominates and
// the flared bottom is cut by a circular mask at the knob's edge — small
// knobs read as domes rather than scaled-down copies.
void drawKnobLayer(juce::Graphics& g, const juce::Image& img, juce::Point<float> c,
                   float radius, float artRadius) {
    const float side = radius * (1024.0f / artRadius);
    const juce::Rectangle<float> d = juce::Rectangle<float>(side, side).withCentre(c);
    if (artRadius < 333.0f) {
        juce::Path clip;
        clip.addEllipse(juce::Rectangle<float>(2.0f * radius, 2.0f * radius).withCentre(c));
        g.saveState();
        g.reduceClipRegion(clip);
        g.drawImage(img, d);
        g.restoreState();
    } else {
        g.drawImage(img, d);
    }
}

// Accent overlay on SHIFT (design/panel-upd.svg, cls-14): #09fc61 with
// mix-blend-mode overlay, a rounded rect on the face, corner ~7.2.
float overlayChannel(float base, float blend) {
    if (base < 0.5f)
        return 2.0f * base * blend;
    return 1.0f - 2.0f * (1.0f - base) * (1.0f - blend);
}

bool inRoundedRect(float x, float y, juce::Rectangle<float> r, float radius) {
    if (!r.contains(x, y))
        return false;
    const float left = r.getX() + radius, right = r.getRight() - radius;
    const float top = r.getY() + radius, bottom = r.getBottom() - radius;
    float dx = 0.0f, dy = 0.0f;
    if (x < left)
        dx = left - x;
    else if (x > right)
        dx = x - right;
    if (y < top)
        dy = top - y;
    else if (y > bottom)
        dy = y - bottom;
    return dx * dx + dy * dy <= radius * radius;
}

const juce::Image& shiftFace(juce::Rectangle<float> face, bool held) {
    const juce::Rectangle<float> frame = theme::buttonFrame(face);
    const int w = juce::roundToInt(frame.getWidth()), h = juce::roundToInt(frame.getHeight());
    static juce::Image normal, pressed;
    juce::Image* dest = &normal;
    const juce::Image* src = &theme::assembledSlices(frame, false);
    if (held) {
        dest = &pressed;
        src = &theme::pressedSlices(frame);
    }
    if (dest->getWidth() == w && dest->getHeight() == h)
        return *dest;
    *dest = src->createCopy();
    const float mx = face.getHeight() * 0.4f, my = face.getHeight() * 0.5f;
    const juce::Rectangle<float> overlay(mx, my, face.getWidth(), face.getHeight());
    constexpr float corner = 7.2f;
    constexpr float accentR = 9.0f / 255.0f, accentG = 252.0f / 255.0f, accentB = 97.0f / 255.0f;
    juce::Image::BitmapData bits(*dest, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < bits.height; ++y)
        for (int x = 0; x < bits.width; ++x) {
            if (!inRoundedRect(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, overlay, corner))
                continue;
            const juce::Colour px = bits.getPixelColour(x, y);
            if (px.getAlpha() == 0)
                continue;
            bits.setPixelColour(x, y, juce::Colour::fromFloatRGBA(overlayChannel(px.getFloatRed(), accentR),
                                                                  overlayChannel(px.getFloatGreen(), accentG),
                                                                  overlayChannel(px.getFloatBlue(), accentB),
                                                                  px.getFloatAlpha()));
        }
    return *dest;
}
}  // namespace

PanelView::PanelView(HydraProcessor& processor)
    : processor_(processor), lights_(std::make_unique<hydra::FrontPanel::Lights>()) {
    for (auto& image : displays_)
        image = juce::Image(juce::Image::RGB, hydra::Oled::kWidth, hydra::Oled::kHeight, true);
    setOpaque(true);
    startTimerHz(60);
}

PanelView::~PanelView() = default;

void PanelView::followShiftKey() {
    const bool down = isMouseOver(true) && juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();
    if (down == shiftKeyHeld_)
        return;
    shiftKeyHeld_ = down;
    if (!shiftLatched_ && !shiftForGesture_) {
        if (down)
            pressShift();
        else
            releaseShift(kShiftTrailMs);
    }
    repaint();
}

void PanelView::timerCallback() {
    followShiftKey();
    if (!processor_.readPanel(*lights_, generation_))
        return;
    const bool first = !live_;
    live_ = true;
    if (!shown_)
        shown_ = std::make_unique<hydra::FrontPanel::Lights>();
    // Only what changed is repainted: the displays that differ, the buttons
    // whose LED level moved by more than a 32nd (the levels are PWM samples and
    // jitter), the encoder rings and pads that differ.
    juce::Rectangle<int> dirty;
    const auto mark = [&](juce::Rectangle<float> canvas) {
        const float sx = static_cast<float>(getWidth()) / kCanvasWidth, sy = static_cast<float>(getHeight()) / kCanvasHeight;
        const juce::Rectangle<int> r(static_cast<int>(std::floor(canvas.getX() * sx)) - 1, static_cast<int>(std::floor(canvas.getY() * sy)) - 1,
                                     static_cast<int>(std::ceil(canvas.getWidth() * sx)) + 3, static_cast<int>(std::ceil(canvas.getHeight() * sy)) + 3);
        dirty = dirty.isEmpty() ? r : dirty.getUnion(r);
    };
    for (int d = 0; d < hydra::PanelDisplays::kCount; ++d) {
        const auto i = static_cast<std::size_t>(d);
        const auto& pixels = lights_->displays[i];
        if (!first && pixels == shown_->displays[i])
            continue;
        shown_->displays[i] = pixels;
        juce::Image::BitmapData bitmap(displays_[i], juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < hydra::Oled::kHeight; ++y)
            for (int x = 0; x < hydra::Oled::kWidth; ++x) {
                const std::uint8_t v = pixels[static_cast<std::size_t>(y * hydra::Oled::kWidth + x)];
                bitmap.setPixelColour(x, y, juce::Colour(v, v, v));
            }
        mark(kDisplays[d].expanded(4));
    }
    for (const Button& b : kButtons) {
        if (b.led < 0)
            continue;
        const auto i = static_cast<std::size_t>(b.led);
        if (!first && std::lround(lights_->buttons[i] * 32) == std::lround(shown_->buttons[i] * 32))
            continue;
        shown_->buttons[i] = lights_->buttons[i];
        mark(buttonBounds(b).translated(0, 4));  // include the drop shadow
    }
    // A partial dirty rect leaves the previous wiper on screen: the masked knob
    // image is not redrawn until the next full repaint (pressing any control).
    bool ringsChanged = false;
    for (int e = 0; e < hydra::FrontPanel::kEncoders; ++e) {
        const auto i = static_cast<std::size_t>(e);
        if (!first && lights_->rings[i] == shown_->rings[i])
            continue;
        shown_->rings[i] = lights_->rings[i];
        ringsChanged = true;
    }
    for (int row = 0; row < hydra::FrontPanel::kPadRows; ++row)
        for (int column = 0; column < hydra::FrontPanel::kPadColumns; ++column) {
            const auto i = static_cast<std::size_t>(row * hydra::FrontPanel::kPadColumns + column);
            if (!first && lights_->pads[i] == shown_->pads[i])
                continue;
            shown_->pads[i] = lights_->pads[i];
            mark(padBounds(row, column).expanded(12, 14));  // the pad's shadow and halo
        }
    if (first || lights_->patchRing != shown_->patchRing) {
        shown_->patchRing = lights_->patchRing;
        const float reach = kDataRadius + 42;  // knob + ring glow
        mark(juce::Rectangle<float>(2 * reach, 2 * reach).withCentre({kDataX, kDataY}));
    }
    if (first || ringsChanged)
        repaint();
    else if (!dirty.isEmpty())
        repaint(dirty);
}

juce::Point<float> PanelView::toCanvas(juce::Point<float> p) const {
    return {p.x * kCanvasWidth / static_cast<float>(getWidth()), p.y * kCanvasHeight / static_cast<float>(getHeight())};
}

void PanelView::paint(juce::Graphics& g) {
    g.fillAll(theme::kBackground);
    g.addTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / kCanvasWidth,
                                                static_cast<float>(getHeight()) / kCanvasHeight));
    paintHeadings(g);
    paintModuleLines(g);
    paintDisplays(g);
    paintButtons(g);
    paintKnobs(g);
    paintEncoders(g);
    paintPads(g);
    if (!live_) {
        g.setColour(theme::kTextDim);
        g.setFont(theme::font(26.0f));
        g.drawText("waiting for the emulator...", kDisplays[0].expanded(200, 0).translated(0, 100),
                   juce::Justification::centred);
    }
}

void PanelView::paintDisplays(juce::Graphics& g) {
    const juce::Graphics::ScopedSaveState save(g);
    // The screens' black glass, behind the OLEDs.
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(kDisplays[0], 8.0f);
    g.fillRoundedRectangle({kSmallDisplayX, kSmallDisplayY, kSmallDisplayW, kSmallDisplayH}, 8.0f);
    g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
    for (int d = 0; d < hydra::PanelDisplays::kCount; ++d) {
        juce::Rectangle<float> r = kDisplays[d];
        if (d == 0)
            r = r.withTrimmedTop(5).withTrimmedBottom(5).withTrimmedLeft(4).withTrimmedRight(4);
        else
            r = r.reduced(3, 2);
        g.setOpacity(1.0f);
        g.drawImage(displays_[static_cast<std::size_t>(d)], r, juce::RectanglePlacement::stretchToFit);
    }
}

void PanelView::paintButtons(juce::Graphics& g) {
    for (const Button& b : kButtons) {
        const juce::Rectangle<float> r = buttonBounds(b);
        const bool held = (active_.kind == Target::Kind::Button && &kButtons[active_.index] == &b) ||
                          (b.id == kShiftButton && (shiftLatched_ || shiftForGesture_ || shiftKeyHeld_));
        if (b.id == kShiftButton) {
            const juce::Rectangle<float> frame = theme::buttonFrame(r);
            g.drawImageAt(shiftFace(r, held), juce::roundToInt(frame.getX()), juce::roundToInt(frame.getY()));
        } else {
            theme::paintButtonFace(g, r, held);
        }
        const float level = b.led >= 0 ? lights_->buttons[static_cast<std::size_t>(b.led)] : 0.0f;
        theme::paintLedBar(g, r, level);
        if (*b.label) {
            const auto text = r.reduced(4, 2).toNearestInt().withTrimmedBottom(6);
            g.setFont(theme::font(b.h > 34.0f ? 13.0f : 12.0f, 0.9f));
            g.setColour(theme::kText);
            g.drawFittedText(b.label, text, juce::Justification::centred, 1);
        }
        for (const Legend& legend : kShiftLegends)
            if (legend.id == b.id) {
                g.setColour(theme::kAccentDim);
                g.setFont(theme::font(12.0f));
                g.drawText(legend.text, juce::Rectangle<float>(r.getX() - 20, r.getBottom() + 1, r.getWidth() + 40, 15),
                           juce::Justification::centred);
            }
        // The Control buttons' numbers: under the upper row, above the lower one.
        if (b.id >= 33 && b.id <= 40) {
            const int number = b.id - 32;
            const float y = number <= 4 ? r.getBottom() + 2 : r.getY() - 20;
            g.setColour(theme::kTextDim);
            g.setFont(theme::font(13.0f));
            g.drawText(juce::String(number), juce::Rectangle<float>(r.getX(), y, r.getWidth(), 15), juce::Justification::centred);
        }
        if (b.id == 61) {  // DOWN: the pair is labelled PAGE
            g.setColour(theme::kAccentDim);
            g.setFont(theme::font(13.0f));
            g.drawText("PAGE", juce::Rectangle<float>(r.getX() - 10, r.getBottom() + 2, r.getWidth() + 20, 15), juce::Justification::centred);
        }
    }
}

// The signal-flow lines of the Module Select section, drawn behind its buttons.
namespace {
// The signal-flow drawing in the updated SVG: absolute M, L and C only.
juce::Path svgPath(const char* src) {
    juce::Path path;
    const char* p = src;
    const auto skip = [&] { while (*p == ' ' || *p == ',') ++p; };
    const auto number = [&] {
        skip();
        char* end = nullptr;
        const float value = std::strtof(p, &end);
        p = end;
        return value;
    };
    const auto more = [&] {
        skip();
        return *p == '-' || *p == '.' || (*p >= '0' && *p <= '9');
    };
    while (*p) {
        skip();
        if (!*p)
            break;
        const char cmd = *p++;
        if (cmd == 'M') {
            const float x = number();
            const float y = number();
            path.startNewSubPath(x, y);
            while (more()) {
                const float lx = number();
                const float ly = number();
                path.lineTo(lx, ly);
            }
        } else if (cmd == 'L') {
            while (more()) {
                const float x = number();
                const float y = number();
                path.lineTo(x, y);
            }
        } else if (cmd == 'C') {
            while (more()) {
                const float x1 = number();
                const float y1 = number();
                const float x2 = number();
                const float y2 = number();
                const float x = number();
                const float y = number();
                path.cubicTo(x1, y1, x2, y2, x, y);
            }
        }
    }
    return path;
}

void strokeSignal(juce::Graphics& g, const char* d) {
    g.strokePath(svgPath(d), juce::PathStrokeType(2.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
}
}  // namespace

void PanelView::paintModuleLines(juce::Graphics& g) {
    // The rounded signal flow from design/panel-upd.svg, behind the buttons.
    g.setColour(theme::kAccent.withAlpha(0.6f));
    g.drawLine(425.5f, 694.0f, 993.5f, 694.0f, 2.4f);
    g.drawLine(425.5f, 925.0f, 993.5f, 925.0f, 2.4f);

    g.setColour(theme::kLine.brighter(0.25f));
    strokeSignal(g, "M121.5 738 L144.5 738 L237.5 738 L256.5 738 L352.5 738 "
                    "C356.9183 738 360.5 741.5817 360.5 746 L360.5 874 "
                    "C360.5 878.4183 356.9183 882 352.5 882 L347.6362 882");
    strokeSignal(g, "M121.5 882 L192.2565 882 C196.6747 882 200.2565 885.5817 200.2565 890 "
                    "L200.2565 915.8974 C200.2565 920.3157 203.8382 923.8974 208.2565 923.8974 "
                    "L352.5 923.8974 C356.9183 923.8974 360.5 920.3157 360.5 915.8974 L360.5 874");
    strokeSignal(g, "M360.5 809 L352.5 809 L256.5 809 L237.5 809 L144.5 809 L121.5 809");
    g.drawLine(360.5f, 809.0f, 369.5f, 809.0f, 2.5f);
    g.drawLine(440.5f, 809.0f, 452.25f, 809.0f, 2.5f);
    g.drawLine(559.25f, 809.0f, 574.5f, 809.0f, 2.5f);
    strokeSignal(g, "M468 775 L460.25 775 C455.8317 775 452.25 778.5817 452.25 783 "
                    "L452.25 837 C452.25 841.4183 455.8317 845 460.25 845 L468 845");
    strokeSignal(g, "M544 775 L551.25 775 C555.6683 775 559.25 778.5817 559.25 783 "
                    "L559.25 837 C559.25 841.4183 555.6683 845 551.25 845 L544 845");
    g.drawLine(506.0f, 794.5f, 506.0f, 825.5f, 2.5f);
    const float dash[] = {6.0f, 5.0f};
    g.drawDashedLine(juce::Line<float>(506.0f, 713.5f, 506.0f, 755.5f), dash, 2, 2.0f);
    g.drawDashedLine(juce::Line<float>(506.0f, 864.5f, 506.0f, 905.5f), dash, 2, 2.0f);
    g.drawDashedLine(juce::Line<float>(610.0f, 713.5f, 610.0f, 789.5f), dash, 2, 2.0f);
    g.drawDashedLine(juce::Line<float>(610.0f, 828.5f, 610.0f, 905.5f), dash, 2, 2.0f);
    g.drawLine(645.5f, 809.0f, 671.5f, 809.0f, 2.5f);
    strokeSignal(g, "M754.5 809 L759.75 809 C764.1683 809 767.75 805.4183 767.75 801 "
                    "L767.75 783 C767.75 778.5817 771.3317 775 775.75 775 L781 775");
    strokeSignal(g, "M887.5 809 L880.25 809 C875.8317 809 872.25 812.5817 872.25 817 "
                    "L872.25 837 C872.25 841.4183 868.6683 845 864.25 845 L857 845");
    g.drawLine(819.0f, 794.5f, 819.0f, 825.5f, 2.5f);
}

void PanelView::paintHeadings(juce::Graphics& g) {
    const juce::Font numberFont = theme::font(11.0f, 1.5f);
    const juce::Font titleFont = theme::font(13.0f, 2.4f);
    for (const Section& section : kSections) {
        const float ruleY = section.y + 12.5f;
        g.setColour(juce::Colours::white.withAlpha(0.14f));
        g.drawLine(section.x, ruleY, section.ruleRight, ruleY, 1.0f);
        g.setColour(theme::kAccent);
        juce::GlyphArrangement number;
        number.addLineOfText(numberFont, section.number, section.x, section.y);
        number.draw(g);
        g.setColour(theme::kText);
        juce::GlyphArrangement title;
        title.addLineOfText(titleFont, section.title, section.x + 26.0f, section.y);
        title.draw(g);
    }
}

namespace {
// The detent of an arp switch position in the shared raster of eight (0 = bottom
// left, clockwise), checked against the firmware's arp edit page.
int arpSlot(int sw, int position) { return sw == hydra::FrontPanel::kArpOctave ? position + 4 : position; }

// The printed position names, in the raster's order (empty: no detent).
const char* const kArpLegends[hydra::FrontPanel::kArpSwitches][8] = {
    {"UP", "DOWN", "UP / DN", "UP & DN", "ORDER", "RANDOM", "CHORD", "PHRASE"},
    {"", "", "", "", "1", "2", "3", "4"},
    {"1/4", "1/8", "1/16", "1/32", "", "", "", ""},
    {"OFF", "52%", "54%", "58%", "62%", "66%", "70%", "75%"},
};

// The printed position names next to an arp switch: four evenly spaced rows
// on each side (the left rows in reverse detent order), each tied to the knob
// by a short horizontal tick beside the text. The current position is printed
// brighter.
void paintArpLegends(juce::Graphics& g, const Knob& k, int position) {
    constexpr float kRow = 20.0f;
    for (int row = 0; row < 4; ++row) {
        const float y = k.y + (static_cast<float>(row) - 1.5f) * kRow;
        for (const bool left : {true, false}) {
            const int slot = left ? 3 - row : row + 4;
            const char* legend = kArpLegends[k.index][slot];
            if (!*legend)
                continue;
            const float side = left ? -1.0f : 1.0f;
            const bool current = slot == position;
            const float near = k.x + side * (k.radius + 6.0f);
            const float far = k.x + side * (k.radius + 17.0f);
            g.setColour(current ? theme::kAccent : theme::kTextDim.withAlpha(0.45f));
            g.drawLine(near, y, far, y, 1.2f);
            g.setColour(current ? juce::Colour(0xfff2f2f2) : theme::kTextDim);
            g.setFont(theme::font(12.0f, 0.3f));
            const juce::Rectangle<float> box =
                left ? juce::Rectangle<float>(far - 4 - 88, y - 9, 88, 18)
                     : juce::Rectangle<float>(far + 4, y - 9, 88, 18);
            g.drawText(legend, box, left ? juce::Justification::centredRight
                                       : juce::Justification::centredLeft);
        }
    }
}
// Master volume as a vertical fader: a capsule well around a pill track, with
// the accent fill rising from the bottom. `radius` is half the track travel.
void paintVolumeSlider(juce::Graphics& g, const Knob& k, float value, juce::StringRef db) {
    const float top = k.y - k.radius, bottom = k.y + k.radius;
    const float trackW = 8.0f;
    const juce::Rectangle<float> track(k.x - trackW * 0.5f, top, trackW, bottom - top);
    const juce::Rectangle<float> well = track.expanded(7.0f, 7.0f);
    g.setColour(theme::kWell);
    g.fillRoundedRectangle(well, well.getWidth() * 0.5f);
    juce::Path slot;
    slot.addRoundedRectangle(track, trackW * 0.5f);
    g.setColour(juce::Colour(0xff35383d));
    g.fillPath(slot);
    const float yValue = bottom - value * (bottom - top);
    if (value > 0.001f) {
        g.saveState();
        g.reduceClipRegion(slot);
        g.setColour(theme::kAccent);
        g.fillRect(track.getX(), yValue, trackW, bottom - yValue);
        g.restoreState();
    }

    const auto centered = [&](const juce::Font& font, juce::StringRef text, float baseline) {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(font, text, k.x - theme::textWidth(font, text) * 0.5f, baseline);
        glyphs.draw(g);
    };
    g.setColour(theme::kTextSoft);
    centered(theme::font(12.0f, 0.8f), "VOL", top - 19.0f);
    g.setColour(theme::kText);
    centered(theme::font(13.0f), db, bottom + 28.0f);
}
}  // namespace

void PanelView::paintKnobs(juce::Graphics& g) {
    for (const Knob& k : kKnobs) {
        float value = 0;
        switch (k.kind) {
        case Knob::Kind::Volume:
            paintVolumeSlider(g, k, processor_.outputGain().convertTo0to1(processor_.outputGain().get()),
                              juce::String(processor_.outputGain().get(), 1) + " dB");
            continue;
        case Knob::Kind::Arp:
            // All four arp switches share one raster of eight detents over 270
            // degrees; OCTAVE uses the right four, DIVISION the left four.
            value = static_cast<float>(arpSlot(k.index, arpPositions_[static_cast<std::size_t>(k.index)])) / 7.0f;
            break;
        case Knob::Kind::Analog: value = knobValues_[static_cast<std::size_t>(k.index)]; break;
        }
        const juce::Point<float> c(k.x, k.y);
        if (k.kind != Knob::Kind::Arp)
            theme::paintValueArc(g, c, k.radius * 1.12f, value);
        const float art = k.kind == Knob::Kind::Arp ? 260.0f : 333.0f;
        drawKnobLayer(g, knobBodyImage(), c, k.radius, art);
        drawKnobLayer(g, knobWiperImage(juce::jlimit(0, 126, juce::roundToInt(value * 126.0f))),
                      c, k.radius, art);
        g.setColour(theme::kTextSoft);
        g.setFont(theme::font(13.0f, 1.1f));
        g.drawText(k.label, juce::Rectangle<float>(150, 18).withCentre(c.translated(0, k.radius + 16)),
                   juce::Justification::centred);
    }
    // The legend lists run over the neighbouring knobs' margins, so
    // they are painted only once every knob body is down.
    for (const Knob& k : kKnobs)
        if (k.kind == Knob::Kind::Arp)
            paintArpLegends(g, k, arpSlot(k.index, arpPositions_[static_cast<std::size_t>(k.index)]));
}

void PanelView::paintEncoders(juce::Graphics& g) {
    const auto encoder = [&g](juce::Point<float> c, float radius, int frame) {
        drawKnobLayer(g, knobBodyImage(), c, radius, 260.0f);
        drawKnobLayer(g, knobWiperImage(frame), c, radius, 260.0f);
    };
    for (int e = 0; e < hydra::FrontPanel::kEncoders; ++e) {
        const juce::Point<float> c = encoderCentre(e);
        // The ring: 15 LEDs over 270 degrees, from bottom left to bottom right.
        int lastLit = -1;
        for (int l = 0; l < hydra::FrontPanel::kRingLeds; ++l)
            if (lights_->rings[static_cast<std::size_t>(e)][static_cast<std::size_t>(l)])
                lastLit = l;
        // The wiper sits at the end of the lit arc; unlit parks at minimum.
        encoder(c, kEncoderRadius, juce::jlimit(0, 126, lastLit * 9));
        // The ring dots sit inside the knob's margins: drawn over it.
        for (int l = 0; l < hydra::FrontPanel::kRingLeds; ++l) {
            const float dotAngle = juce::MathConstants<float>::pi * (-0.75f + 1.5f * static_cast<float>(l) / 14.0f);
            const bool lit = lights_->rings[static_cast<std::size_t>(e)][static_cast<std::size_t>(l)];
            theme::paintRingDot(g, c.getPointOnCircumference(kEncoderRadius + 5.5f, dotAngle), 2.8f, lit);
        }
    }
    // The data encoder is endless: a body without a position mark.
    drawKnobLayer(g, knobBodyImage(), {kDataX, kDataY}, kDataRadius, 333.0f);
    // The patch knob's light ring: a flat grey outer ring with the preset's own
    // glow colour inside it (the firmware defines it per preset). When the LED
    // is off the ring keeps a dim accent tint so it always reads.
    {
        const juce::Colour ringColour = lights_->patchRing != 0
                                            ? juce::Colour(lights_->patchRing | 0xff000000u)
                                            : theme::kAccent.withAlpha(0.45f);
        g.setColour(juce::Colour(0xff55585e));
        g.drawEllipse(juce::Rectangle<float>(2 * (kDataRadius + 18), 2 * (kDataRadius + 18))
                          .withCentre({kDataX, kDataY}),
                      2.6f);
        g.setColour(ringColour);
        g.drawEllipse(juce::Rectangle<float>(2 * (kDataRadius + 10), 2 * (kDataRadius + 10))
                          .withCentre({kDataX, kDataY}),
                      11.0f);
    }
}


namespace {
// The pad faces the design assigns to each position (bottom row first, as
// kPadLegends). Unlit pads show the colourway so the panel keeps the design's
// layout; lit pads show the LED's actual colour.
const juce::Colour kPadIdle[3][8] = {
    {juce::Colour(0xff9cb53d), juce::Colour(0xffb57a3a), juce::Colour(0xff3d9c7a), juce::Colour(0xffb57a3a),
     juce::Colour(0xff3d9c7a), juce::Colour(0xff3d9c7a), juce::Colour(0xffb57a3a), juce::Colour(0xff3d9c7a)},
    {juce::Colour(0xffb57a3a), juce::Colour(0xff3d9c7a), juce::Colour(0xffb57a3a), juce::Colour(0xff3d9c7a),
     juce::Colour(0xff9cb53d), juce::Colour(0xffb57a3a), juce::Colour(0xff3d9c7a), juce::Colour(0xffb57a3a)},
    {juce::Colour(0xff3d9c7a), juce::Colour(0xff3d9c7a), juce::Colour(0xffb57a3a), juce::Colour(0xff3d9c7a),
     juce::Colour(0xffb57a3a), juce::Colour(0xff3d9c7a), juce::Colour(0xffb57a3a), juce::Colour(0xff3d9c7a)},
};

juce::Colour padColour(std::uint32_t rgb) {
    const int r8 = static_cast<int>(rgb >> 16 & 0xFF), g8 = static_cast<int>(rgb >> 8 & 0xFF), b8 = static_cast<int>(rgb & 0xFF);
    if (r8 + g8 + b8 == 0)
        return juce::Colour(0xff353637);
    if (r8 > g8 + 40 && r8 > b8 + 40)
        return g8 > 110 ? juce::Colour(0xff9cb53d) : juce::Colour(0xffb57a3a);
    return juce::Colour(0xff3d9c7a);
}

void paintPadFace(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour face, bool held, float corner) {
    g.setColour(held ? face.darker(0.25f) : face);
    g.fillRoundedRectangle(r, corner);
    // A 1px light rim down the sides and a dark lip along the bottom.
    const float lip = 6.57f;
    const juce::Rectangle<float> cap = r.withTrimmedBottom(lip);
    juce::Path shade;
    shade.addRoundedRectangle(r, corner);
    shade.addRoundedRectangle(cap, corner);
    shade.setUsingNonZeroWinding(false);
    g.setColour(theme::kWell.withAlpha(0.13f));
    g.fillPath(shade);
    juce::Path rim;
    rim.addRoundedRectangle(cap, corner);
    rim.addRoundedRectangle(cap.reduced(1.0f), corner - 1.0f);
    rim.setUsingNonZeroWinding(false);
    g.setColour(juce::Colours::white.withAlpha(0.13f));
    g.fillPath(rim);
}
}  // namespace

void PanelView::paintPads(juce::Graphics& g) {
    for (int row = 0; row < hydra::FrontPanel::kPadRows; ++row)
        for (int column = 0; column < hydra::FrontPanel::kPadColumns; ++column) {
            const juce::Rectangle<float> r = padBounds(row, column);
            const std::uint32_t rgb = lights_->pads[static_cast<std::size_t>(row * hydra::FrontPanel::kPadColumns + column)];
            const bool lit = rgb != 0;
            const float corner = 9.0f;
            const bool held = active_.kind == Target::Kind::Pad &&
                              active_.index == row * hydra::FrontPanel::kPadColumns + column;
            paintPadFace(g, r, lit ? padColour(rgb) : kPadIdle[row][column], held, corner);
            // The lit pads' corner pip.
            if (lit) {
                const int r8 = static_cast<int>(rgb >> 16 & 0xFF), g8 = static_cast<int>(rgb >> 8 & 0xFF),
                          b8 = static_cast<int>(rgb & 0xFF);
                if (r8 > 100 && g8 > 60 && r8 > b8 * 2) {
                    g.setColour(theme::kAccentBright);
                    g.fillEllipse(juce::Rectangle<float>(6, 6).withCentre({r.getRight() - 10.0f, r.getY() + 10.0f}));
                }
            }
            const char* legend = kPadLegends[row][column];
            if (*legend) {
                const juce::Rectangle<float> label(r.getX() - 10, r.getBottom() + 4, r.getWidth() + 20, 16);
                g.setFont(theme::font(12.0f));
                if (std::strcmp(legend, "KEY") == 0) {  // in a small red-outlined pill
                    const juce::Rectangle<float> box = label.withSizeKeepingCentre(44, 18);
                    g.setColour(theme::kAccent.withAlpha(0.7f));
                    g.drawRoundedRectangle(box, box.getHeight() / 2, 1.2f);
                    g.setColour(theme::kText);
                    g.drawText(legend, box, juce::Justification::centred);
                } else {
                    g.setColour(theme::kAccentDim);
                    g.drawText(legend, label, juce::Justification::centred);
                }
            }
        }
}

PanelView::Target PanelView::targetAt(juce::Point<float> p) const {
    for (std::size_t i = 0; i < std::size(kButtons); ++i)
        if (buttonBounds(kButtons[i]).contains(p))
            return {Target::Kind::Button, static_cast<int>(i)};
    for (std::size_t i = 0; i < std::size(kKnobs); ++i) {
        const Knob& k = kKnobs[i];
        if (k.kind == Knob::Kind::Volume) {
            const juce::Rectangle<float> hit(k.x - 34.0f, k.y - k.radius, 68.0f, k.radius * 2.0f);
            if (hit.contains(p))
                return {Target::Kind::Knob, static_cast<int>(i)};
        } else if (p.getDistanceFrom({k.x, k.y}) <= k.radius + 4)
            return {Target::Kind::Knob, static_cast<int>(i)};
    }
    for (int e = 0; e < hydra::FrontPanel::kEncoders; ++e)
        if (p.getDistanceFrom(encoderCentre(e)) <= kEncoderRadius + 12)
            return {Target::Kind::Encoder, e};
    if (p.getDistanceFrom({kDataX, kDataY}) <= kDataRadius)
        return {Target::Kind::DataEncoder, 0};
    for (int row = 0; row < hydra::FrontPanel::kPadRows; ++row)
        for (int column = 0; column < hydra::FrontPanel::kPadColumns; ++column)
            if (padBounds(row, column).contains(p))
                return {Target::Kind::Pad, row * hydra::FrontPanel::kPadColumns + column};
    return {};
}

void PanelView::setKnob(int index, float value) {
    const Knob& k = kKnobs[index];
    value = juce::jlimit(0.0f, 1.0f, value);
    switch (k.kind) {
    case Knob::Kind::Volume: processor_.outputGain().setValueNotifyingHost(value); break;
    case Knob::Kind::Arp: {
        const int positions = hydra::FrontPanel::kArpPositions[static_cast<std::size_t>(k.index)];
        const int position = juce::roundToInt(value * static_cast<float>(positions - 1));
        if (position != arpPositions_[static_cast<std::size_t>(k.index)]) {
            arpPositions_[static_cast<std::size_t>(k.index)] = position;
            processor_.panelInput({Input::Kind::ArpSwitch, k.index, position, 0});
        }
        break;
    }
    case Knob::Kind::Analog:
        knobValues_[static_cast<std::size_t>(k.index)] = value;
        processor_.panelInput({Input::Kind::Knob, k.index, 0, value});
        break;
    }
    repaint();
}

void PanelView::turn(const Target& target, int detents) {
    if (detents == 0)
        return;
    switch (target.kind) {
    case Target::Kind::Encoder: processor_.panelInput({Input::Kind::Encoder, target.index, detents, 0}); break;
    case Target::Kind::DataEncoder: processor_.panelInput({Input::Kind::DataEncoder, 0, detents, 0}); break;
    case Target::Kind::Knob: {
        const Knob& k = kKnobs[target.index];
        if (k.kind == Knob::Kind::Arp) {
            const int positions = hydra::FrontPanel::kArpPositions[static_cast<std::size_t>(k.index)];
            const int position = arpPositions_[static_cast<std::size_t>(k.index)] + detents;
            setKnob(target.index, static_cast<float>(position) / static_cast<float>(positions - 1));
        } else {
            const float value = k.kind == Knob::Kind::Volume ? processor_.outputGain().convertTo0to1(processor_.outputGain().get())
                                                             : knobValues_[static_cast<std::size_t>(k.index)];
            setKnob(target.index, value + 0.02f * static_cast<float>(detents));
        }
        break;
    }
    default: break;
    }
}

void PanelView::mouseDown(const juce::MouseEvent& e) {
    active_ = targetAt(toCanvas(e.position));
    dragStartY_ = e.position.y;
    dragAccumulator_ = 0;
    switch (active_.kind) {
    case Target::Kind::Button: {
        const int id = kButtons[active_.index].id;
        if (id == kShiftButton) {
            if (shiftLatched_)
                releaseShift(0);
            else
                pressShift();
            shiftLatched_ = !shiftLatched_;
            break;
        }
        int delay = 0;
        if (shiftKeyHeld_) {
            delay = kShiftLeadMs;  // SHIFT is down already; keep the order safe
        } else if (!shiftLatched_ && e.mods.isShiftDown()) {
            pressShift();
            shiftForGesture_ = true;
            delay = kShiftLeadMs;
        }
        processor_.panelInput({Input::Kind::Button, id, 1, 0, delay});
        break;
    }
    case Target::Kind::Encoder:
    case Target::Kind::DataEncoder:
        if (!shiftLatched_ && !shiftKeyHeld_ && e.mods.isShiftDown()) {
            pressShift();
            shiftForGesture_ = true;
        }
        break;
    case Target::Kind::Pad: {
        // Pressure from where the pad is hit: harder towards its top.
        const int row = active_.index / hydra::FrontPanel::kPadColumns, column = active_.index % hydra::FrontPanel::kPadColumns;
        const juce::Rectangle<float> r = padBounds(row, column);
        const float pressure = juce::jlimit(0.2f, 1.0f, 1.0f - (toCanvas(e.position).y - r.getY()) / r.getHeight() * 0.8f);
        processor_.panelInput({Input::Kind::Pad, active_.index, 0, pressure});
        break;
    }
    case Target::Kind::Knob: {
        const Knob& k = kKnobs[active_.index];
        dragValue_ = k.kind == Knob::Kind::Volume ? processor_.outputGain().convertTo0to1(processor_.outputGain().get())
                     : k.kind == Knob::Kind::Arp
                         ? static_cast<float>(arpPositions_[static_cast<std::size_t>(k.index)]) /
                               static_cast<float>(hydra::FrontPanel::kArpPositions[static_cast<std::size_t>(k.index)] - 1)
                         : knobValues_[static_cast<std::size_t>(k.index)];
        break;
    }
    default: break;
    }
    repaint();
}

void PanelView::mouseDrag(const juce::MouseEvent& e) {
    const float dy = dragStartY_ - e.position.y;  // up is positive
    switch (active_.kind) {
    case Target::Kind::Knob: {
        const Knob& k = kKnobs[active_.index];
        if (k.kind == Knob::Kind::Volume) {
            const float travel = 2.0f * k.radius * static_cast<float>(getHeight()) / kCanvasHeight;
            setKnob(active_.index, dragValue_ + dy / travel);
        } else {
            setKnob(active_.index, dragValue_ + dy / 200.0f);
        }
        break;
    }
    case Target::Kind::Encoder:
    case Target::Kind::DataEncoder: {
        // One detent per 8 pixels of vertical movement.
        const float total = dy / 8.0f;
        const int detents = static_cast<int>(total - dragAccumulator_);
        dragAccumulator_ += static_cast<float>(detents);
        turn(active_, detents);
        break;
    }
    default: break;
    }
}

void PanelView::mouseUp(const juce::MouseEvent&) {
    switch (active_.kind) {
    case Target::Kind::Button: {
        const int id = kButtons[active_.index].id;
        if (id == kShiftButton)
            break;  // latched or released by the click itself
        const int delay = shiftForGesture_ || shiftKeyHeld_ ? kShiftLeadMs : 0;
        processor_.panelInput({Input::Kind::Button, id, 0, 0, delay});
        if (shiftLatched_ || shiftForGesture_) {
            releaseShift(delay + kShiftTrailMs);
            shiftLatched_ = shiftForGesture_ = false;
        }
        break;
    }
    case Target::Kind::Encoder:
    case Target::Kind::DataEncoder:
        if (shiftForGesture_) {
            releaseShift(kShiftTrailMs + 100);  // after the last detents
            shiftForGesture_ = false;
        }
        break;
    case Target::Kind::Pad: processor_.panelInput({Input::Kind::Pad, active_.index, 0, 0.0f}); break;
    default: break;
    }
    active_ = {};
    repaint();
}

void PanelView::pressShift() { processor_.panelInput({Input::Kind::Button, kShiftButton, 1, 0, 0}); }

void PanelView::releaseShift(int delayMs) { processor_.panelInput({Input::Kind::Button, kShiftButton, 0, 0, delayMs}); }

void PanelView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    // A wheel notch (0.1-0.25 per notch, depending on the platform) is one detent.
    wheelAccumulator_ += wheel.deltaY * 8.0f;
    const int detents = static_cast<int>(wheelAccumulator_);
    wheelAccumulator_ -= static_cast<float>(detents);
    turn(targetAt(toCanvas(e.position)), detents);
}

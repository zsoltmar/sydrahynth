#include "PanelView.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <tuple>

#include "HydraProcessor.h"
#include "LogoData.h"
#include "Theme.h"

namespace {
using Input = hydra::Emulator::PanelInput;
constexpr int kShiftButton = 62;
// A button pressed together with SHIFT goes down this much after SHIFT, and
// SHIFT is let go this much after the button: the firmware scans the buttons
// every 5 ms and must see SHIFT held when the other button arrives.
constexpr int kShiftLeadMs = 20, kShiftTrailMs = 80;

// Buttons: label, keyscan id, LED index (-1: none), centre and size on the canvas.
struct Button {
    const char* label;
    int id, led;
    float x, y, w = 70, h = 36;
};
const Button kButtons[] = {
    // Arpeggiator
    {"ON", 26, 30, 337, 334, 100, 39}, {"LATCH", 28, 31, 484, 334, 100, 39},
    {"TRIPLET", 46, 32, 634, 334, 100, 39}, {"TAP TEMPO", 48, 29, 783, 334, 100, 39},
    // Knob assignment for cutoff and resonance
    {"FILTER 1", 79, 26, 805, 485, 76, 39}, {"FILTER 2", 80, 27, 805, 537, 76, 39},
    // Patch and system
    {"BROWSE", 45, 34, 963, 129, 76, 39}, {"SAVE", 50, 33, 963, 214, 76, 39}, {"SYSTEM", 27, 38, 963, 288, 76, 39},
    {"INIT", 44, 35, 963, 353, 76, 39}, {"RANDOM", 59, 36, 963, 428, 76, 39}, {"SHIFT", 62, 39, 963, 522, 76, 39},
    // Navigation
    {"<", 51, -1, 1060, 429, 62, 39}, {">", 55, -1, 1170, 429, 62, 39}, {"HOME", 49, -1, 1114, 522, 171, 39},
    {"EXIT", 7, 45, 1858, 124, 100, 39}, {"VOICE", 56, 46, 1858, 213, 100, 39}, {"UP", 60, 47, 1858, 290, 100, 39},
    {"DOWN", 61, 48, 1858, 355, 100, 39}, {"MACRO ASSIGN", 57, 49, 1858, 427, 100, 39}, {"MOD MATRIX", 58, 58, 1858, 517, 100, 39},
    // Encoder push buttons
    {"", 33, 50, 1356, 217, 66, 32}, {"", 34, 51, 1474, 217, 66, 32}, {"", 35, 52, 1594, 217, 66, 32},
    {"", 36, 53, 1711, 217, 66, 32}, {"", 37, 54, 1355, 431, 66, 32}, {"", 38, 55, 1475, 431, 66, 32},
    {"", 39, 56, 1594, 431, 66, 32}, {"", 40, 57, 1712, 431, 66, 32},
    // Module buttons
    {"OSC 1", 11, 0, 86, 725, 71, 39}, {"MUTANT 1", 14, 3, 191, 725, 93, 39}, {"MUTANT 2", 15, 4, 303, 725, 93, 39},
    {"OSC 2", 12, 1, 86, 796, 71, 39}, {"MUTANT 3", 16, 5, 191, 796, 93, 39}, {"MUTANT 4", 3, 6, 303, 796, 93, 39},
    {"OSC 3", 13, 2, 86, 869, 71, 39}, {"RING-NOISE", 17, 7, 295, 869, 109, 39},
    {"MIXER", 0, 8, 401, 796, 71, 39}, {"FILTER 1", 24, 19, 506, 762, 76, 39}, {"FILTER 2", 25, 20, 506, 832, 76, 39},
    {"AMP", 1, 21, 610, 796, 71, 39}, {"PRE-FX", 2, 22, 713, 796, 83, 39}, {"DELAY", 4, 23, 819, 762, 76, 39},
    {"REVERB", 5, 24, 819, 832, 76, 39}, {"POST-FX", 6, 25, 934, 796, 93, 39},
    {"ENV 1", 70, 9, 506, 681, 71, 39}, {"ENV 2", 71, 10, 610, 681, 71, 39}, {"ENV 3", 72, 11, 714, 681, 71, 39},
    {"ENV 4", 73, 12, 819, 681, 71, 39}, {"ENV 5", 18, 13, 923, 681, 71, 39},
    {"LFO 1", 66, 14, 506, 912, 71, 39}, {"LFO 2", 67, 15, 610, 912, 71, 39}, {"LFO 3", 68, 16, 714, 912, 71, 39},
    {"LFO 4", 69, 17, 819, 912, 71, 39}, {"LFO 5", 47, 18, 923, 912, 71, 39},
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

// The section cards (with their headings on the top edge, as in the design).
struct Card {
    const char* heading;  // nullptr: no heading
    float x, y, w, h;
};
const Card kCards[] = {
    {nullptr, 34, 169, 156, 419},          // master volume
    {"ARPEGGIATOR", 219, 169, 654, 234},   //
    {"FILTER CONTROLS", 219, 436, 654, 154},
    {"MODULE SELECT", 35, 624, 966, 351},
    {"MAIN SYSTEMS", 904, 55, 326, 535},
    {"MASTER CONTROL", 1262, 55, 672, 536},
};

// Knobs: master volume (the plugin's output gain), the arpeggiator switches and
// the analog knobs.
struct Knob {
    enum class Kind { Volume, Arp, Analog } kind;
    const char* label;
    int index;  // switch or knob
    float x, y, radius;
};
const Knob kKnobs[] = {
    {Knob::Kind::Volume, "MASTER VOLUME", 0, 111, 377, 42},
    {Knob::Kind::Arp, "MODE", hydra::FrontPanel::kArpMode, 336, 244, 34},
    {Knob::Kind::Arp, "OCTAVE", hydra::FrontPanel::kArpOctave, 484, 244, 34},
    {Knob::Kind::Arp, "DIVISION", hydra::FrontPanel::kArpDivision, 634, 244, 34},
    {Knob::Kind::Arp, "SWING", hydra::FrontPanel::kArpSwing, 781, 244, 34},
    {Knob::Kind::Analog, "CUTOFF", hydra::FrontPanel::kCutoff, 330, 506, 38},
    {Knob::Kind::Analog, "RESONANCE", hydra::FrontPanel::kResonance, 489, 506, 38},
    {Knob::Kind::Analog, "DRIVE / MORPH", hydra::FrontPanel::kDriveMorph, 653, 506, 38},
};

constexpr float kEncoderX[4] = {1356, 1474, 1594, 1711}, kEncoderY[2] = {136, 520}, kEncoderRadius = 26;
constexpr float kDataX = 1114, kDataY = 166, kDataRadius = 72;

// Displays: the main one, then the four small ones (top-left corner and size);
// the small ones sit in one continuous black screen.
const juce::Rectangle<float> kDisplays[] = {
    {1020, 270, 190, 105}, {1284, 292, 120, 60}, {1410, 292, 120, 60}, {1536, 292, 120, 60}, {1662, 292, 120, 60}};

// The screen behind the small displays (they share one black area).
constexpr float kSmallDisplayX = 1284, kSmallDisplayY = 270, kSmallDisplayW = 498, kSmallDisplayH = 105;

constexpr float kPadW = 83, kPadH = 64, kPadX = 1064.4f, kPadPitch = 108.4f;
constexpr float kPadY[3] = {885, 786, 687};  // row centres, bottom row first
juce::Rectangle<float> padBounds(int row, int column) {
    return {kPadX + kPadPitch * static_cast<float>(column), kPadY[row] - kPadH / 2, kPadW, kPadH};
}

juce::Rectangle<float> buttonBounds(const Button& b) { return {b.x - b.w / 2, b.y - b.h / 2, b.w, b.h}; }
juce::Point<float> encoderCentre(int e) { return {kEncoderX[e & 3], kEncoderY[e >> 2]}; }


// The logo's path data (M, L, C, Z only), mapped from the design's coordinate
// space onto the canvas.
const juce::Path& logoPath() {
    static const juce::Path path = [] {
        juce::Path p;
        const char* s = kLogoPathData;
        const auto number = [&s] {
            while (*s == ' ')
                ++s;
            char* end = nullptr;
            const float v = std::strtof(s, &end);
            s = end;
            return v;
        };
        while (*s != '\0') {
            const char command = *s++;
            if (command == 'M')
                p.startNewSubPath(number(), number());
            else if (command == 'L')
                p.lineTo(number(), number());
            else if (command == 'C')
                p.cubicTo(number(), number(), number(), number(), number(), number());
            else if (command == 'Z')
                p.closeSubPath();
        }
        p.applyTransform(juce::AffineTransform(0.554f, 0.0f, 33.4f, 0.0f, 0.554f, 37.4f));
        return p;
    }();
    return path;
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
    for (int e = 0; e < hydra::FrontPanel::kEncoders; ++e) {
        const auto i = static_cast<std::size_t>(e);
        if (!first && lights_->rings[i] == shown_->rings[i])
            continue;
        shown_->rings[i] = lights_->rings[i];
        const float reach = kEncoderRadius + 18;  // knob shadow + ring dots
        mark(juce::Rectangle<float>(2 * reach, 2 * reach).withCentre(encoderCentre(e)));
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
    if (first)
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
    // The logo in the top left corner: the supplied artwork drawn with the
    // design's own treatment — a black ghost fill, a faint white outline, and
    // a black outline offset 1px up so the dark line sits above the light.
    {
        const juce::Path& logo = logoPath();
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.fillPath(logo);
        g.setColour(juce::Colours::white.withAlpha(0.09f));
        g.strokePath(logo, juce::PathStrokeType(1.0f), juce::AffineTransform::translation(1.0f, 1.0f));
        g.setColour(juce::Colours::black.withAlpha(0.9f));
        g.strokePath(logo, juce::PathStrokeType(1.0f), juce::AffineTransform::translation(0.0f, -1.0f));
    }
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
        theme::paintButtonFace(g, r, held, r.getHeight() > 34.0f ? 6.0f : 9.0f);
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
                g.drawText(legend.text, juce::Rectangle<float>(r.getX() - 20, r.getBottom() + 4, r.getWidth() + 40, 15),
                           juce::Justification::centred);
            }
        // The Control buttons' numbers: under the upper row, above the lower one.
        if (b.id >= 33 && b.id <= 40) {
            const int number = b.id - 32;
            const float y = number <= 4 ? r.getBottom() + 5 : r.getY() - 20;
            g.setColour(theme::kTextDim);
            g.setFont(theme::font(13.0f));
            g.drawText(juce::String(number), juce::Rectangle<float>(r.getX(), y, r.getWidth(), 15), juce::Justification::centred);
        }
        if (b.id == 61) {  // DOWN: the pair is labelled PAGE
            g.setColour(theme::kTextDim);
            g.setFont(theme::font(13.0f));
            g.drawText("PAGE", juce::Rectangle<float>(r.getX() - 10, r.getBottom() + 5, r.getWidth() + 20, 15), juce::Justification::centred);
        }
    }
}

// The signal-flow lines of the Module Select section, drawn behind its buttons.
void PanelView::paintModuleLines(juce::Graphics& g) {
    const auto box = [](int id) {
        for (const Button& b : kButtons)
            if (b.id == id && (b.y > 640))  // the module buttons (FILTER 1/2 also exist in the filter section)
                return buttonBounds(b);
        return juce::Rectangle<float>();
    };
    enum : int { kOsc1 = 11, kOsc2 = 12, kOsc3 = 13, kMut1 = 14, kMut2 = 15, kMut3 = 16, kMut4 = 3, kRing = 17, kMixer = 0,
                 kFilter1 = 24, kFilter2 = 25, kAmp = 1, kPreFx = 2, kDelay = 4, kReverb = 5, kPostFx = 6,
                 kEnv1 = 70, kEnv2 = 71, kEnv5 = 18, kLfo1 = 66, kLfo2 = 67, kLfo5 = 47 };
    const auto line = [&g](float x1, float y1, float x2, float y2) { g.drawLine(x1, y1, x2, y2, 2.5f); };
    const auto dashed = [&g](float x1, float y1, float x2, float y2, float width, float dash, float gap) {
        const float pattern[] = {dash, gap};
        g.drawDashedLine(juce::Line<float>(x1, y1, x2, y2), pattern, 2, width);
    };

    // Red lines behind the envelope and LFO rows.
    g.setColour(theme::kAccent.withAlpha(0.6f));
    for (const int id : {kEnv1, kLfo1}) {
        const float y = box(id).getCentreY();
        g.drawLine(box(id).getX() - 45, y, box(id == kEnv1 ? kEnv5 : kLfo5).getRight() + 35, y, 2.4f);
    }

    g.setColour(theme::kLine.brighter(0.25f));
    // Oscillators to mutants, and on to the mixer bus.
    for (const auto& [a, b] : {std::pair{kOsc1, kMut1}, {kMut1, kMut2}, {kOsc2, kMut3}, {kMut3, kMut4}, {kOsc3, kRing}})
        line(box(a).getRight(), box(a).getCentreY(), box(b).getX(), box(a).getCentreY());
    const float mixBus = (box(kMut2).getRight() + box(kMixer).getX()) / 2;
    for (const int id : {kMut2, kMut4, kRing})
        line(box(id).getRight(), box(id).getCentreY(), mixBus, box(id).getCentreY());
    line(mixBus, box(kMut2).getCentreY(), mixBus, box(kRing).getCentreY());
    line(mixBus, box(kMixer).getCentreY(), box(kMixer).getX(), box(kMixer).getCentreY());

    // Mixer to the filters (series or parallel) and on to the amp.
    const float inBus = (box(kMixer).getRight() + box(kFilter1).getX()) / 2;
    const float outBus = (box(kFilter1).getRight() + box(kAmp).getX()) / 2;
    line(box(kMixer).getRight(), box(kMixer).getCentreY(), inBus, box(kMixer).getCentreY());
    line(outBus, box(kAmp).getCentreY(), box(kAmp).getX(), box(kAmp).getCentreY());
    for (const float bus : {inBus, outBus}) {
        line(bus, box(kFilter1).getCentreY(), bus, box(kFilter2).getCentreY());
        for (const int id : {kFilter1, kFilter2})
            line(bus, box(id).getCentreY(), bus == inBus ? box(id).getX() : box(id).getRight(), box(id).getCentreY());
    }
    line(box(kFilter1).getCentreX(), box(kFilter1).getBottom(), box(kFilter1).getCentreX(), box(kFilter2).getY());

    // The pre-wired modulators: ENV 1 / LFO 1 to the filters, ENV 2 / LFO 2 to the amp.
    for (const auto& [top, middleTop, middleBottom, bottom] :
         {std::tuple{kEnv1, kFilter1, kFilter2, kLfo1}, std::tuple{kEnv2, kAmp, kAmp, kLfo2}}) {
        const float x = box(middleTop).getCentreX();
        dashed(x, box(top).getBottom(), x, box(middleTop).getY(), 2.0f, 6.0f, 5.0f);
        dashed(x, box(middleBottom).getBottom(), x, box(bottom).getY(), 2.0f, 6.0f, 5.0f);
    }

    // Amp, pre-FX, delay and reverb (in series or parallel), post-FX.
    line(box(kAmp).getRight(), box(kAmp).getCentreY(), box(kPreFx).getX(), box(kPreFx).getCentreY());
    const float fxIn = (box(kPreFx).getRight() + box(kDelay).getX()) / 2;
    const float fxOut = (box(kDelay).getRight() + box(kPostFx).getX()) / 2;
    line(box(kPreFx).getRight(), box(kPreFx).getCentreY(), fxIn, box(kPreFx).getCentreY());
    line(fxOut, box(kPostFx).getCentreY(), box(kPostFx).getX(), box(kPostFx).getCentreY());
    for (const float bus : {fxIn, fxOut}) {
        line(bus, box(kDelay).getCentreY(), bus, box(kReverb).getCentreY());
        for (const int id : {kDelay, kReverb})
            line(bus, box(id).getCentreY(), bus == fxIn ? box(id).getX() : box(id).getRight(), box(id).getCentreY());
    }
    line(box(kDelay).getCentreX(), box(kDelay).getBottom(), box(kDelay).getCentreX(), box(kReverb).getY());
}

void PanelView::paintHeadings(juce::Graphics& g) {
    for (const Card& card : kCards) {
        const juce::Rectangle<float> r(card.x, card.y, card.w, card.h);
        theme::paintCard(g, r);
        if (card.heading != nullptr)
            theme::paintCardHeading(g, r, card.heading, theme::kBackground);
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

// The position names next to an arp switch: a four-row list on each side (the
// left rows in reverse detent order), each with a short dash pointing to the
// knob. The switch's current position is printed brighter.
void paintArpLegends(juce::Graphics& g, const Knob& k, int position) {
    constexpr float kRow = 15.0f;
    const float dashNear = k.radius + 8.0f, dashFar = k.radius + 15.0f;
    for (int row = 0; row < 4; ++row) {
        const float y = k.y + (static_cast<float>(row) - 1.5f) * kRow;
        for (const bool left : {true, false}) {
            const int slot = left ? 3 - row : row + 4;
            const char* legend = kArpLegends[k.index][slot];
            if (!*legend)
                continue;
            const float side = left ? -1.0f : 1.0f;
            const juce::Point<float> near(k.x + side * dashNear, y), far(k.x + side * dashFar, y);
            const bool current = slot == position;
            g.setColour(current ? theme::kAccent : theme::kTextDim.withAlpha(0.5f));
            g.drawLine(juce::Line<float>(near, far), 1.4f);
            g.setColour(current ? juce::Colour(0xfff2f2f2) : theme::kTextDim);
            g.setFont(theme::font(12.0f, 0.3f));
            const juce::Rectangle<float> box = left ? juce::Rectangle<float>(far.x - 4 - 88, y - 9, 88, 18)
                                                    : juce::Rectangle<float>(far.x + 4, y - 9, 88, 18);
            g.drawText(legend, box, left ? juce::Justification::centredRight : juce::Justification::centredLeft);
        }
    }
}
}  // namespace

void PanelView::paintKnobs(juce::Graphics& g) {
    for (const Knob& k : kKnobs) {
        float value = 0;
        juce::String text;
        switch (k.kind) {
        case Knob::Kind::Volume:
            value = processor_.outputGain().convertTo0to1(processor_.outputGain().get());
            text = juce::String(processor_.outputGain().get(), 1) + " dB";
            break;
        case Knob::Kind::Arp:
            // All four arp switches share one raster of eight detents over 270
            // degrees; OCTAVE uses the right four, DIVISION the left four.
            value = static_cast<float>(arpSlot(k.index, arpPositions_[static_cast<std::size_t>(k.index)])) / 7.0f;
            break;
        case Knob::Kind::Analog: value = knobValues_[static_cast<std::size_t>(k.index)]; break;
        }
        const juce::Point<float> c(k.x, k.y);
        const float angle = juce::MathConstants<float>::pi * (-0.75f + 1.5f * value);
        theme::paintKnobBody(g, c, k.radius);
        if (k.kind != Knob::Kind::Arp)
            theme::paintValueArc(g, c, k.radius * 1.12f, value);
        theme::paintKnobTick(g, c, k.radius, angle);
        g.setColour(theme::kTextSoft);
        if (k.kind == Knob::Kind::Volume) {
            g.setFont(theme::font(13.0f, 1.1f));
            g.drawText(k.label, juce::Rectangle<float>(150, 18).withCentre(c.translated(0, -k.radius - 18)),
                       juce::Justification::centred);
            g.setColour(theme::kText);
            g.setFont(theme::font(14.0f));
            g.drawText(text, juce::Rectangle<float>(150, 18).withCentre(c.translated(0, k.radius + 22)),
                       juce::Justification::centred);
        } else {  // as on the hardware: the name under the knob
            g.setFont(theme::font(13.0f, 1.1f));
            g.drawText(k.label, juce::Rectangle<float>(150, 18).withCentre(c.translated(0, k.radius + 16)),
                       juce::Justification::centred);
        }
    }
    // The legend lists run over the neighbouring knobs' margins, so
    // they are painted only once every knob body is down.
    for (const Knob& k : kKnobs)
        if (k.kind == Knob::Kind::Arp)
            paintArpLegends(g, k, arpSlot(k.index, arpPositions_[static_cast<std::size_t>(k.index)]));
}

void PanelView::paintEncoders(juce::Graphics& g) {
    const auto encoder = [&g](juce::Point<float> c, float radius, float angle) {
        theme::paintKnobBody(g, c, radius, 0.78f);
        theme::paintKnobTick(g, c, radius, angle, 0.78f);
    };
    for (int e = 0; e < hydra::FrontPanel::kEncoders; ++e) {
        const juce::Point<float> c = encoderCentre(e);
        // The ring: 15 LEDs over 270 degrees, from bottom left to bottom right.
        int lastLit = -1;
        for (int l = 0; l < hydra::FrontPanel::kRingLeds; ++l)
            if (lights_->rings[static_cast<std::size_t>(e)][static_cast<std::size_t>(l)])
                lastLit = l;
        // The cap's tick sits at the end of the lit arc.
        const float angle = lastLit < 0 ? -juce::MathConstants<float>::pi * 0.75f
                                        : juce::MathConstants<float>::pi * (-0.75f + 1.5f * static_cast<float>(lastLit) / 14.0f);
        encoder(c, kEncoderRadius, angle);
        // The ring dots sit inside the knob's margins: drawn over it.
        for (int l = 0; l < hydra::FrontPanel::kRingLeds; ++l) {
            const float dotAngle = juce::MathConstants<float>::pi * (-0.75f + 1.5f * static_cast<float>(l) / 14.0f);
            const bool lit = lights_->rings[static_cast<std::size_t>(e)][static_cast<std::size_t>(l)];
            theme::paintRingDot(g, c.getPointOnCircumference(kEncoderRadius + 5.5f, dotAngle), 2.8f, lit);
        }
    }
    // The data encoder is endless: a body without a position mark.
    theme::paintKnobBody(g, {kDataX, kDataY}, kDataRadius, 0.78f);
    // The patch knob's light ring: a flat grey outer ring with a flat red ring
    // inside it. The LED drives the brightness, but the ring always reads.
    {
        const float level = lights_->patchRing != 0 ? 1.0f : 0.45f;
        g.setColour(juce::Colour(0xff55585e));
        g.drawEllipse(juce::Rectangle<float>(2 * (kDataRadius + 18), 2 * (kDataRadius + 18))
                          .withCentre({kDataX, kDataY}),
                      2.6f);
        g.setColour(theme::kAccent.withAlpha(level));
        g.drawEllipse(juce::Rectangle<float>(2 * (kDataRadius + 10), 2 * (kDataRadius + 10))
                          .withCentre({kDataX, kDataY}),
                      4.0f);
    }
    g.setColour(juce::Colour(0xff9ea2a8));
    g.setFont(theme::font(13.0f, 1.6f));
    g.drawText("PATCH", juce::Rectangle<float>(90, 18).withCentre({kDataX, kDataY + 6}), juce::Justification::centred);
}


namespace {
// The pad faces the design assigns to each position (bottom row first, as
// kPadLegends). Unlit pads show the colourway so the panel keeps the design's
// layout; lit pads show the LED's actual colour.
const juce::Colour kPadIdle[3][8] = {
    {juce::Colour(0xffc9a13a), juce::Colour(0xffb54a4a), juce::Colour(0xff4b89bf), juce::Colour(0xffb54a4a),
     juce::Colour(0xff4b89bf), juce::Colour(0xff4b89bf), juce::Colour(0xffb54a4a), juce::Colour(0xff4b89bf)},
    {juce::Colour(0xffb54a4a), juce::Colour(0xff4b89bf), juce::Colour(0xffb54a4a), juce::Colour(0xff4b89bf),
     juce::Colour(0xffc9a13a), juce::Colour(0xffb54a4a), juce::Colour(0xff4b89bf), juce::Colour(0xffb54a4a)},
    {juce::Colour(0xff4b89bf), juce::Colour(0xff4b89bf), juce::Colour(0xffb54a4a), juce::Colour(0xff4b89bf),
     juce::Colour(0xffb54a4a), juce::Colour(0xff4b89bf), juce::Colour(0xffb54a4a), juce::Colour(0xff4b89bf)},
};

juce::Colour padColour(std::uint32_t rgb) {
    const int r8 = static_cast<int>(rgb >> 16 & 0xFF), g8 = static_cast<int>(rgb >> 8 & 0xFF), b8 = static_cast<int>(rgb & 0xFF);
    if (r8 + g8 + b8 == 0)
        return juce::Colour(0xff353637);
    if (r8 > g8 + 40 && r8 > b8 + 40)
        return g8 > 110 ? juce::Colour(0xffc9a13a) : juce::Colour(0xffb54a4a);
    return juce::Colour(0xff4b89bf);
}

void paintPadFace(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour face, bool held, float corner) {
    g.setColour(held ? face.darker(0.25f) : face);
    g.fillRoundedRectangle(r, corner);
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
    for (std::size_t i = 0; i < std::size(kKnobs); ++i)
        if (p.getDistanceFrom({kKnobs[i].x, kKnobs[i].y}) <= kKnobs[i].radius + 4)
            return {Target::Kind::Knob, static_cast<int>(i)};
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
    case Target::Kind::Knob: setKnob(active_.index, dragValue_ + dy / 200.0f); break;
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

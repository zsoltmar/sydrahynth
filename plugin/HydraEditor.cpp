#include "HydraEditor.h"

#include <algorithm>
#include <cmath>

#include "HydraProcessor.h"
#include "Theme.h"

HydraEditor::HydraEditor(HydraProcessor& owner) : AudioProcessorEditor(owner), processor_(owner), browser_(owner), panel_(owner) {
    setLookAndFeel(&lnf_);
    title_.setText("S Y D R A H Y N T H", juce::dontSendNotification);
    title_.setFont(theme::font(14.0f, 1.0f));
    title_.setColour(juce::Label::textColourId, theme::kTextFaint);
    firmware_.setFont(theme::font(13.0f));
    firmware_.setColour(juce::Label::textColourId, theme::kAccentDim);
    status_.setFont(theme::font(13.0f));
    status_.setColour(juce::Label::textColourId, theme::kTextDim);
    status_.setJustificationType(juce::Justification::centredRight);
    for (juce::Label* label : {&title_, &firmware_, &status_})
        addAndMakeVisible(*label);
    addAndMakeVisible(chooseFirmware_);
    chooseFirmware_.onClick = [this] { chooseFirmware(); };
    for (std::size_t i = 0; i < HydraProcessor::kLatencyFrames.size(); ++i)
        latency_.addItem("Latency " + juce::String(std::lround(1000.0 * HydraProcessor::kLatencyFrames[i] / 44100.0)) + " ms",
                         static_cast<int>(i) + 1);
    latency_.setSelectedId(processor_.latencyChoice() + 1, juce::dontSendNotification);
    latency_.setTooltip("How far the emulator runs ahead of the host. Lower is more direct; raise it if the audio crackles.");
    latency_.onChange = [this] { processor_.setLatencyChoice(latency_.getSelectedId() - 1); };
    addAndMakeVisible(latency_);
    togglePatches_.setClickingTogglesState(true);
    togglePatches_.setTooltip("Show or hide the patch browser");
    togglePatches_.onClick = [this] { showBrowser(togglePatches_.getToggleState(), true); };
    addAndMakeVisible(togglePatches_);
    addChildComponent(browser_);
    addAndMakeVisible(panel_);
    // The panel keeps its proportions; the status bar and the browser have fixed sizes.
    setConstrainer(&constrainer_);
    setResizable(true, true);
    const bool browser = processor_.browserVisible();
    showBrowser(browser, false);
    const juce::Point<int> size = constrainer_.sizeFor(1372);
    setSize(size.x, size.y);
    timerCallback();
    startTimerHz(4);
}

HydraEditor::~HydraEditor() = default;

void PanelConstrainer::setLayout(int extraWidth, int extraHeight, double aspect, int minPanelWidth, int maxPanelWidth) {
    extraWidth_ = extraWidth;
    extraHeight_ = extraHeight;
    aspect_ = aspect;
    minPanelWidth_ = minPanelWidth;
    maxPanelWidth_ = maxPanelWidth;
    const juce::Point<int> smallest = sizeFor(minPanelWidth), largest = sizeFor(maxPanelWidth);
    setSizeLimits(smallest.x, smallest.y, largest.x, largest.y);
}

juce::Point<int> PanelConstrainer::sizeFor(int panelWidth) const {
    return {panelWidth + extraWidth_, static_cast<int>(std::lround(panelWidth / aspect_)) + extraHeight_};
}

void PanelConstrainer::checkBounds(juce::Rectangle<int>& bounds, const juce::Rectangle<int>&, const juce::Rectangle<int>&,
                                   bool isStretchingTop, bool isStretchingLeft, bool isStretchingBottom, bool isStretchingRight) {
    // The panel width the drag asks for: from the width when a side is dragged,
    // from the height when the top or bottom is; for a corner (or a host's
    // resize request, which names no edge) the size on the aspect ratio's line
    // nearest to the one proposed (stateless, so the steps of a drag do not
    // fight, and it follows the mouse both ways).
    const bool horizontal = isStretchingLeft || isStretchingRight, vertical = isStretchingTop || isStretchingBottom;
    const int fromWidth = bounds.getWidth() - extraWidth_;
    const int fromHeight = static_cast<int>(std::lround((bounds.getHeight() - extraHeight_) * aspect_));
    int panelWidth = fromWidth;
    if (vertical && !horizontal)
        panelWidth = fromHeight;
    else if (vertical == horizontal) {
        const double a2 = aspect_ * aspect_;
        panelWidth = static_cast<int>(std::lround((fromWidth + fromHeight / a2) / (1.0 + 1.0 / a2)));
    }
    panelWidth = std::clamp(panelWidth, minPanelWidth_, maxPanelWidth_);
    const juce::Point<int> size = sizeFor(panelWidth);
    // Keep the edges that are not dragged where they are.
    const int x = isStretchingLeft ? bounds.getRight() - size.x : bounds.getX();
    const int y = isStretchingTop ? bounds.getBottom() - size.y : bounds.getY();
    bounds = {x, y, size.x, size.y};
}

void HydraEditor::updateResizeLimits() {
    constrainer_.setLayout(browser_.isVisible() ? kBrowserWidth : 0, kBarHeight, PanelView::kCanvasWidth / PanelView::kCanvasHeight, 700, 2400);
}

void HydraEditor::showBrowser(bool visible, bool resizeWindow) {
    togglePatches_.setToggleState(visible, juce::dontSendNotification);
    if (browser_.isVisible() == visible && resizeWindow)
        return;
    browser_.setVisible(visible);
    processor_.setBrowserVisible(visible);
    updateResizeLimits();
    if (resizeWindow) {
        const juce::Point<int> size = constrainer_.sizeFor(getWidth() - (visible ? 0 : kBrowserWidth));
        setSize(size.x, size.y);
    }
    else
        resized();
}

void HydraEditor::paint(juce::Graphics& g) {
    g.fillAll(theme::kBackground);
    // The status bar's bottom hairline.
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.fillRect(0.0f, kBarHeight - 1.0f, static_cast<float>(getWidth()), 1.0f);
    // The live dot at the right end of the status text.
    const juce::String text = status_.getText();
    if (text.isNotEmpty()) {
        const float width = theme::textWidth(status_.getFont(), text);
        const float x = status_.getRight() - width - 15.0f, y = kBarHeight / 2.0f;
        g.setColour(theme::kAccent);
        g.fillEllipse(x, y - 3.5f, 7.0f, 7.0f);
    }
}

void HydraEditor::resized() {
    auto area = getLocalBounds();
    auto bar = area.removeFromTop(kBarHeight).reduced(14, 6);
    title_.setBounds(bar.removeFromLeft(185));
    togglePatches_.setBounds(bar.removeFromLeft(96));
    bar.removeFromLeft(12);
    chooseFirmware_.setBounds(bar.removeFromRight(110));
    bar.removeFromRight(8);
    latency_.setBounds(bar.removeFromRight(150));
    bar.removeFromRight(8);
    status_.setBounds(bar.removeFromRight(320));
    firmware_.setBounds(bar);
    if (browser_.isVisible())
        browser_.setBounds(area.removeFromLeft(kBrowserWidth));
    panel_.setBounds(area);
}

void HydraEditor::timerCallback() {
    const juce::File file = processor_.firmwareFile();
    // Only a missing firmware is worth a line; the path is the button's tooltip.
    const bool missing = !file.existsAsFile();
    firmware_.setText(!missing ? juce::String()
                      : file == juce::File() ? juce::String("No firmware chosen: pick the Desktop 2.2.0 firmware (.dat) from the ASM firmware pack")
                                             : "Firmware not found: " + file.getFullPathName(),
                      juce::dontSendNotification);
    chooseFirmware_.setTooltip(missing ? juce::String("Choose the firmware file") : "Firmware: " + file.getFullPathName());
    status_.setText(processor_.statusText(), juce::dontSendNotification);
    // The live dot sits just left of the text and slides with its width:
    // repaint the strip covering every position it can take so its old spot
    // is cleared.
    repaint(status_.getBounds().translated(-24, 0).withHeight(kBarHeight));
}

void HydraEditor::chooseFirmware() {
    chooser_ = std::make_unique<juce::FileChooser>("Desktop 2.2.0 firmware (.dat)", processor_.firmwareFile(), "*.dat");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this](const juce::FileChooser& chooser) {
                              const juce::File file = chooser.getResult();
                              if (file.existsAsFile())
                                  processor_.setFirmwareFile(file);
                          });
}

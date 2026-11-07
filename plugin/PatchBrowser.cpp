#include "PatchBrowser.h"

#include "HydraProcessor.h"
#include "Theme.h"

namespace {
// The bank letter buttons inside their dark track: the selected segment has a
// flat face and a red edge.
class BankLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool, bool) override {
        if (!b.getToggleState())
            return;
        const juce::Rectangle<float> r = b.getLocalBounds().toFloat().reduced(2.5f, 3.0f);
        g.setColour(juce::Colour(0xff2e2f32));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(theme::kAccent.withAlpha(0.8f));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override {
        g.setColour(b.getToggleState() ? theme::kText : theme::kTextDim);
        g.setFont(theme::font(13.0f));
        g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }
};
}  // namespace

PatchBrowser::PatchBrowser(HydraProcessor& processor) : processor_(processor) {
    static BankLookAndFeel bankLnf;
    for (int b = 0; b < Patches::kBanks; ++b) {
        juce::TextButton& button = bankButtons_[static_cast<std::size_t>(b)];
        button.setLookAndFeel(&bankLnf);
        button.setButtonText(juce::String::charToString(static_cast<juce::juce_wchar>('A' + b)));
        button.setClickingTogglesState(true);
        button.setRadioGroupId(1);
        button.setConnectedEdges((b > 0 ? juce::Button::ConnectedOnLeft : 0) |
                                 (b < Patches::kBanks - 1 ? juce::Button::ConnectedOnRight : 0));
        // The group turns a button off with the same notification, which fires
        // onClick again: only an ON click switches the shown bank.
        button.onClick = [this, &button, b] { if (button.getToggleState()) showBank(b); };
        addAndMakeVisible(button);
    }
    bankButtons_[0].setToggleState(true, juce::dontSendNotification);
    list_.setModel(this);
    list_.setRowHeight(25);
    // The list's well and border are rounded, drawn in paint()/paintOverChildren.
    list_.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list_.setOutlineThickness(0);
    list_.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff454547));
    list_.getVerticalScrollBar().setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(list_);
    loadBank_.setTooltip("Send the patches of a .hydra bank (or a .patch) to the shown bank of the instrument");
    loadBank_.onClick = [this] { chooseBank(); };
    addAndMakeVisible(loadBank_);
    timerCallback();
    startTimerHz(10);
}

PatchBrowser::~PatchBrowser() = default;

void PatchBrowser::paint(juce::Graphics& g) {
    const juce::Rectangle<float> card = getLocalBounds().toFloat().reduced(5);
    theme::paintCard(g, card, 12.0f, theme::kBrowserCard);
    // The current patch: its slot in red, the name in bright text.
    if (currentSlot_.isNotEmpty()) {
        float x = card.getX() + 15;
        const float y = card.getY() + 24;
        g.setColour(theme::kAccent);
        g.setFont(theme::font(13.5f, 2.0f));
        g.drawText(currentSlot_, juce::Rectangle<float>(x, y - 7, 56, 14), juce::Justification::centredLeft);
        g.setColour(theme::kTextSoft);
        g.setFont(theme::font(17.0f, 0.6f));
        g.drawFittedText(currentName_, juce::Rectangle<int>(static_cast<int>(x + 60), static_cast<int>(y - 11),
                                                          static_cast<int>(card.getRight() - 10 - x - 60), 22),
                         juce::Justification::centredLeft, 1);
    }
    // The dark track behind the bank letter buttons, and the list's well.
    const juce::Rectangle<float> track = bankBar_.toFloat();
    g.setColour(theme::kWell);
    g.fillRoundedRectangle(track, track.getHeight() / 2);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(track, track.getHeight() / 2, 1.0f);
    g.setColour(theme::kWellDeep);
    g.fillRoundedRectangle(listWell_.toFloat(), 8.0f);
}

// The list's rounded border, drawn over the rows so their fills stay clipped
// to the corner radius. Rows are clipped to a rectangle, so a flat band first
// masks the wedges their fills would poke into at the corners.
void PatchBrowser::paintOverChildren(juce::Graphics& g) {
    const juce::Rectangle<float> r = listWell_.toFloat();
    juce::Path ring;
    ring.addRectangle(r);
    ring.addRoundedRectangle(r.reduced(3.0f), 5.5f);
    ring.setUsingNonZeroWinding(false);
    g.setColour(juce::Colour(0xff191818));
    g.fillPath(ring);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
}

void PatchBrowser::resized() {
    auto area = getLocalBounds().reduced(12, 12);
    area.removeFromTop(30);  // the header, drawn in paint()
    area.removeFromTop(4);
    bankBar_ = area.removeFromTop(28);
    auto banks = bankBar_.reduced(3);
    const int width = banks.getWidth() / Patches::kBanks;
    for (auto& button : bankButtons_)
        button.setBounds(banks.removeFromLeft(width));
    area.removeFromTop(6);
    loadBank_.setBounds(area.removeFromBottom(30));
    area.removeFromBottom(8);
    listWell_ = area;
    list_.setBounds(area.reduced(0, 5));  // pad the rows off the well's rounded caps
}

juce::String PatchBrowser::slotName(int bank, int slot) {
    return juce::String::charToString(static_cast<juce::juce_wchar>('A' + bank)) + juce::String(slot + 1).paddedLeft('0', 3);
}

void PatchBrowser::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) {
    const std::string& name = patches_.names[static_cast<std::size_t>(bank_ * Patches::kSlots + row)];
    if (selected) {
        const juce::Rectangle<float> r(4.0f, 1.5f, width - 8.0f, height - 3.0f);
        g.setColour(juce::Colour(0xff2e2f32));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(theme::kAccent.withAlpha(0.8f));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
    } else {
        g.fillAll(row % 2 ? juce::Colour(0xff0e0d0d) : juce::Colour(0xff0d0c0d));
    }
    g.setFont(theme::font(13.5f));
    g.setColour(selected ? theme::kText : (name.empty() ? theme::kTextFaint : theme::kTextDim));
    g.drawText(slotName(bank_, row), 14, 0, 46, height, juce::Justification::centredLeft);
    g.setColour(selected ? juce::Colour(0xfff0f0f0) : (name.empty() ? theme::kTextFaint : theme::kTextSoft));
    g.drawText(name.empty() ? juce::String("(empty)") : juce::String(name), 62, 0, width - 70, height,
               juce::Justification::centredLeft, true);
}

void PatchBrowser::selectedRowsChanged(int lastRowSelected) {
    if (updating_ || lastRowSelected < 0)
        return;
    const int index = bank_ * Patches::kSlots + lastRowSelected;
    if (patches_.names[static_cast<std::size_t>(index)].empty() || !live_) {
        markCurrent();  // a blank slot cannot be selected (it would load silence)
        return;
    }
    if (index != patches_.current)
        processor_.selectPatch(bank_, lastRowSelected);
}

void PatchBrowser::showBank(int bank) {
    bank_ = bank;
    bankButtons_[static_cast<std::size_t>(bank)].setToggleState(true, juce::dontSendNotification);
    list_.updateContent();
    markCurrent();
    list_.repaint();
}

// Selects the current patch's row (when its bank is shown) without selecting it again.
void PatchBrowser::markCurrent() {
    updating_ = true;
    if (patches_.current >= 0 && patches_.current / Patches::kSlots == bank_)
        list_.selectRow(patches_.current % Patches::kSlots);
    else
        list_.deselectAllRows();
    updating_ = false;
}

void PatchBrowser::timerCallback() {
    if (!processor_.readPatches(patches_, generation_))
        return;
    live_ = true;
    if (patches_.current < 0) {
        currentSlot_ = "PATCH";
        currentName_ = "no patch loaded";
    } else {
        currentSlot_ = slotName(patches_.current / Patches::kSlots, patches_.current % Patches::kSlots);
        currentName_ = patches_.currentName;
    }
    repaint();
    if (patches_.current != lastCurrent_ && patches_.current >= 0) {
        lastCurrent_ = patches_.current;
        showBank(patches_.current / Patches::kSlots);  // follow patch changes made on the panel
        return;
    }
    list_.updateContent();
    markCurrent();
    list_.repaint();
}

void PatchBrowser::chooseBank() {
    chooser_ = std::make_unique<juce::FileChooser>("Bank (.hydra) or patch (.patch)", juce::File(), "*.hydra;*.patch");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this](const juce::FileChooser& chooser) {
                              const juce::File file = chooser.getResult();
                              if (!file.existsAsFile())
                                  return;
                              const int bank = bank_;
                              const juce::String bankName = juce::String::charToString(static_cast<juce::juce_wchar>('A' + bank));
                              juce::AlertWindow::showOkCancelBox(
                                  juce::MessageBoxIconType::QuestionIcon, "Load bank",
                                  "Send " + file.getFileName() + " to bank " + bankName + "? Patches in the same slots are overwritten.",
                                  "Load", "Cancel", this, juce::ModalCallbackFunction::create([this, file, bank](int result) {
                                      if (result != 1)
                                          return;
                                      const juce::String error = processor_.loadBank(file, bank);
                                      if (error.isNotEmpty())
                                          juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Load bank", error);
                                  }));
                          });
}

/*
  ==============================================================================
    VT-2R
    Plugin Editor (UI) Implementation

    Debug Mode: Cmd+Drag to move knobs, Opt+Drag to resize
  ==============================================================================
*/

#include "PluginEditor.h"
#include "BinaryData.h"
#include "PluginProcessor.h"

// Debug Mode: 1=Layout Config, 0=Normal
#define VT2B_DEBUG_MODE 0

// Debug defaults
#if VT2B_DEBUG_MODE
static int g_debugDriveX = 250;
static int g_debugDriveY = 550;
static int g_debugMixX = 774;
static int g_debugMixY = 550;
static int g_debugKnobSize = 250;
#endif

//==============================================================================
// VT2BImageKnob Implementation
//==============================================================================

VT2BImageKnob::VT2BImageKnob() { setRepaintsOnMouseActivity(true); }

VT2BImageKnob::~VT2BImageKnob() {}

void VT2BImageKnob::setImage(const juce::Image &image) {
  knobImage = image;
  repaint();
}

void VT2BImageKnob::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();
  auto centre = bounds.getCentre();

  if (knobImage.isValid()) {
    float normalizedValue =
        static_cast<float>((value - minValue) / (maxValue - minValue));
    float angle = startAngle + normalizedValue * (endAngle - startAngle);

    float knobSize = juce::jmin(bounds.getWidth(), bounds.getHeight());
    float scale = knobSize / static_cast<float>(knobImage.getWidth());

    juce::AffineTransform transform =
        juce::AffineTransform::rotation(
            angle, static_cast<float>(knobImage.getWidth()) / 2.0f,
            static_cast<float>(knobImage.getHeight()) / 2.0f)
            .scaled(scale)
            .translated(centre.x - (knobImage.getWidth() * scale) / 2.0f,
                        centre.y - (knobImage.getHeight() * scale) / 2.0f);

    g.drawImageTransformed(knobImage, transform, false);
  }

#if VT2B_DEBUG_MODE
  g.setColour(juce::Colours::red.withAlpha(0.5f));
  g.drawRect(getLocalBounds(), 2);
  g.setColour(juce::Colours::yellow);
  g.fillEllipse(centre.x - 3, centre.y - 3, 6, 6);
#endif
}

void VT2BImageKnob::resized() {}

void VT2BImageKnob::setRange(double min, double max, double interval) {
  minValue = min;
  maxValue = max;
  defaultValue = (min + max) / 2.0;
  juce::ignoreUnused(interval);
}

void VT2BImageKnob::setValue(double newValue,
                             juce::NotificationType notification) {
  value = juce::jlimit(minValue, maxValue, newValue);
  repaint();

  if (notification != juce::dontSendNotification && onValueChange)
    onValueChange();
}

double VT2BImageKnob::getValue() const { return value; }

void VT2BImageKnob::setLabel(const juce::String &labelText) {
  label = labelText;
  repaint();
}

void VT2BImageKnob::setRotationRange(float startAngleRadians,
                                     float endAngleRadians) {
  startAngle = startAngleRadians;
  endAngle = endAngleRadians;
}

void VT2BImageKnob::mouseDown(const juce::MouseEvent &event) {
#if VT2B_DEBUG_MODE
  if (event.mods.isCommandDown() || event.mods.isAltDown()) {
    debugMode = true;
    debugDragStartX = event.getScreenX();
    debugDragStartY = event.getScreenY();
    return;
  }
#endif
  dragStartValue = value;
  dragStartY = event.y;
}

void VT2BImageKnob::mouseDrag(const juce::MouseEvent &event) {
#if VT2B_DEBUG_MODE
  if (debugMode) {
    int dx = event.getScreenX() - debugDragStartX;
    int dy = event.getScreenY() - debugDragStartY;

    if (event.mods.isCommandDown()) {
      if (label == "DRIVE") {
        g_debugDriveX += dx;
        g_debugDriveY += dy;
      } else {
        g_debugMixX += dx;
        g_debugMixY += dy;
      }
    } else if (event.mods.isAltDown()) {
      g_debugKnobSize += dx;
      g_debugKnobSize = juce::jlimit(50, 400, g_debugKnobSize);
    }

    debugDragStartX = event.getScreenX();
    debugDragStartY = event.getScreenY();

    if (auto *parent = getParentComponent()) {
      parent->resized();
      parent->repaint();
    }
    return;
  }
#endif

  float sensitivity = event.mods.isShiftDown() ? 0.002f : 0.01f;
  double delta = static_cast<double>(dragStartY - event.y) * sensitivity *
                 (maxValue - minValue);
  setValue(dragStartValue + delta);
}

void VT2BImageKnob::mouseUp(const juce::MouseEvent &) {
#if VT2B_DEBUG_MODE
  if (debugMode) {
    debugMode = false;
    DBG("// ===== KNOB POSITIONS =====");
    DBG("// DRIVE: x=" << g_debugDriveX << ", y=" << g_debugDriveY);
    DBG("// MIX: x=" << g_debugMixX << ", y=" << g_debugMixY);
    DBG("// Size: " << g_debugKnobSize);
  }
#endif
}

void VT2BImageKnob::mouseDoubleClick(const juce::MouseEvent &) {
  setValue(defaultValue);
}

void VT2BImageKnob::mouseWheelMove(const juce::MouseEvent &,
                                   const juce::MouseWheelDetails &wheel) {
  double delta = wheel.deltaY * (maxValue - minValue) * 0.05;
  setValue(value + delta);
}

//==============================================================================
// VT2BBlackEditor Implementation
//==============================================================================

VT2BBlackEditor::VT2BBlackEditor(VT2BBlackProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
  loadImages();

  // Set Size to match Background (1024x866)
  if (backgroundImage.isValid())
    setSize(backgroundImage.getWidth(), backgroundImage.getHeight());
  else
    setSize(1024, 866);

  // Drive Knob
  driveKnob.setImage(knobImage);
  driveKnob.setLabel("DRIVE");
  driveKnob.setRange(0.0, 100.0, 0.1);
  driveKnob.setValue(0.0);
  driveKnob.setRotationRange(-2.35619f, 2.35619f); // -135 to 135 steps
  addAndMakeVisible(driveKnob);

  // Mix Knob
  mixKnob.setImage(knobImage);
  mixKnob.setLabel("MIX");
  mixKnob.setRange(0.0, 100.0, 1.0);
  mixKnob.setValue(100.0);
  mixKnob.setRotationRange(-2.35619f, 2.35619f);
  addAndMakeVisible(mixKnob);

  // Internal Sliders for Attachment
  // Both now 0-100 to match Processor
  driveSlider.setRange(0.0, 100.0);
  mixSlider.setRange(0.0, 100.0);

  // Sync Logic (Direct 1:1 mapping now)
  driveKnob.onValueChange = [this]() {
    driveSlider.setValue(driveKnob.getValue(), juce::sendNotificationSync);
  };
  mixKnob.onValueChange = [this]() {
    mixSlider.setValue(mixKnob.getValue(), juce::sendNotificationSync);
  };

  driveSlider.onValueChange = [this]() {
    driveKnob.setValue(driveSlider.getValue(), juce::dontSendNotification);
  };
  mixSlider.onValueChange = [this]() {
    mixKnob.setValue(mixSlider.getValue(), juce::dontSendNotification);
  };

  // Attachments
  driveAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          audioProcessor.getParameters(), "drive", driveSlider);
  mixAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          audioProcessor.getParameters(), "mix", mixSlider);
}

VT2BBlackEditor::~VT2BBlackEditor() {
  driveAttachment.reset();
  mixAttachment.reset();
}

void VT2BBlackEditor::loadImages() {
  backgroundImage = juce::ImageCache::getFromMemory(
      BinaryData::background_png, BinaryData::background_pngSize);

  knobImage = juce::ImageCache::getFromMemory(BinaryData::knob_png,
                                              BinaryData::knob_pngSize);
}

void VT2BBlackEditor::paint(juce::Graphics &g) {
  if (backgroundImage.isValid()) {
    g.drawImage(backgroundImage, getLocalBounds().toFloat());
  } else {
    g.fillAll(juce::Colour(0xff881111)); // Red fallback
  }

#if VT2B_DEBUG_MODE
  g.setColour(juce::Colours::yellow);
  g.setFont(14.0f);
  g.drawText("DEBUG MODE", 10, 10, getWidth(), 20, juce::Justification::left);
#endif
}

void VT2BBlackEditor::resized() {
#if VT2B_DEBUG_MODE
  driveKnob.setBounds(g_debugDriveX - g_debugKnobSize / 2, g_debugDriveY,
                      g_debugKnobSize, g_debugKnobSize);
  mixKnob.setBounds(g_debugMixX - g_debugKnobSize / 2, g_debugMixY,
                    g_debugKnobSize, g_debugKnobSize);
#else
  // User-specified coordinates
  // DRIVE: x=216, y=626
  // MIX: x=809, y=626
  // Size: 206

  int knobSize = 206;

  // DRIVE knob (center at 216, 626)
  driveKnob.setBounds(216 - knobSize / 2, 626 - knobSize / 2, knobSize,
                      knobSize);

  // MIX knob (center at 809, 626)
  mixKnob.setBounds(809 - knobSize / 2, 626 - knobSize / 2, knobSize, knobSize);
#endif
}

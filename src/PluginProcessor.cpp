/*
  ==============================================================================
    VT-2R - EMU AUDIO
    Aggressive Saturation Plugin

    "攻めるためのサチュレーター"

    Character: Punchy / Aggressive / Forward
     Circuit: Transformer + Solid State
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
// Constants for VT-2R
namespace VT2RConstants {
// Drive Range
constexpr float kDriveMin = 0.0f;
constexpr float kDriveMax = 100.0f; // User sees 0-100
constexpr float kDriveDefault = 0.0f;

// Mix Range
constexpr float kMixMin = 0.0f;
constexpr float kMixMax = 100.0f;
constexpr float kMixDefault = 100.0f;

// DSP Constants
constexpr float kPreEmphasisFreq = 2000.0f; // 2kHz
constexpr float kPreEmphasisQ = 0.7f;
constexpr float kMaxPreEmphasisGainDb = 9.0f; // Boost mids up to 9dB

// Saturation Curve
// Higher drive = steeper curve
constexpr float kSaturationSteepnessBase = 1.0f;
constexpr float kSaturationSteepnessMax = 5.0f;

} // namespace VT2RConstants

//==============================================================================
VT2BBlackProcessor::VT2BBlackProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("VT2R"),
                 createParameterLayout()) {
  driveParameter = parameters.getRawParameterValue("drive");
  mixParameter = parameters.getRawParameterValue("mix");
}

VT2BBlackProcessor::~VT2BBlackProcessor() {}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
VT2BBlackProcessor::createParameterLayout() {
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

  // Drive 0-100
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"drive", 1}, "Drive",
      juce::NormalisableRange<float>(VT2RConstants::kDriveMin,
                                     VT2RConstants::kDriveMax, 0.1f),
      VT2RConstants::kDriveDefault,
      juce::AudioParameterFloatAttributes().withLabel("")));

  // Mix 0-100
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"mix", 1}, "Mix",
      juce::NormalisableRange<float>(VT2RConstants::kMixMin,
                                     VT2RConstants::kMixMax, 1.0f),
      VT2RConstants::kMixDefault,
      juce::AudioParameterFloatAttributes().withLabel("%")));

  return {params.begin(), params.end()};
}

//==============================================================================
const juce::String VT2BBlackProcessor::getName() const {
  return JucePlugin_Name;
}

bool VT2BBlackProcessor::acceptsMidi() const { return false; }
bool VT2BBlackProcessor::producesMidi() const { return false; }
bool VT2BBlackProcessor::isMidiEffect() const { return false; }
double VT2BBlackProcessor::getTailLengthSeconds() const { return 0.0; }

int VT2BBlackProcessor::getNumPrograms() { return 1; }
int VT2BBlackProcessor::getCurrentProgram() { return 0; }
void VT2BBlackProcessor::setCurrentProgram(int) {}
const juce::String VT2BBlackProcessor::getProgramName(int) { return {}; }
void VT2BBlackProcessor::changeProgramName(int, const juce::String &) {}

//==============================================================================
void VT2BBlackProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  currentSampleRate = sampleRate;

  smoothedDrive.reset(sampleRate, 0.02); // 20ms smoothing
  smoothedMix.reset(sampleRate, 0.02);

  // Reset Filter States
  midBoostStateL = {0, 0};
  midBoostStateR = {0, 0};
  lastInL = 0;
  lastInR = 0;
}

void VT2BBlackProcessor::releaseResources() {}

bool VT2BBlackProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;

  return true;
}

//==============================================================================
void VT2BBlackProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                      juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  juce::ignoreUnused(midiMessages);

  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();

  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());

  float drive = *driveParameter;
  float mix = *mixParameter / 100.0f;

  smoothedDrive.setTargetValue(drive);
  smoothedMix.setTargetValue(mix);

  auto *channelDataL = buffer.getWritePointer(0);
  auto *channelDataR =
      totalNumInputChannels > 1 ? buffer.getWritePointer(1) : nullptr;

  for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
    float currentDrive = smoothedDrive.getNextValue();
    float currentMix = smoothedMix.getNextValue();

    float dryL = channelDataL[sample];
    float dryR = channelDataR ? channelDataR[sample] : dryL;

    // --- Signal Chain ---

    // 1. Input Gain & Pre-Emphasis
    // Boost mids to make them hit saturation harder ("Forward" character)
    float wetL = processPreEmphasis(dryL, currentDrive, midBoostStateL);

    // 2. Saturation (Steep Sigmoid / Solid State)
    wetL = processSaturation(wetL, currentDrive);

    // 3. Output makeup
    wetL *= calculateMakeupGain(currentDrive);

    // Right Channel
    float wetR = dryR;
    if (channelDataR != nullptr) {
      wetR = processPreEmphasis(dryR, currentDrive, midBoostStateR);
      wetR = processSaturation(wetR, currentDrive);
      wetR *= calculateMakeupGain(currentDrive);
    } else {
      wetR = wetL;
    }

    // Mix
    channelDataL[sample] = dryL * (1.0f - currentMix) + wetL * currentMix;
    if (channelDataR != nullptr)
      channelDataR[sample] = dryR * (1.0f - currentMix) + wetR * currentMix;
  }
}

//==============================================================================
// DSP Implementations

float VT2BBlackProcessor::processPreEmphasis(float input, float drive,
                                             FilterState &state) {
  // Drive 0-100 -> Gain 0dB to +9dB
  float normDrive = drive / 100.0f;
  float gainDb = normDrive * VT2RConstants::kMaxPreEmphasisGainDb;

  // Calculate Coefficients for Peaking EQ
  // Simplified RBJ Biquad for fixed freq/Q but variable gain
  // In a real loop, optimization would compute coeffs only when drive changes
  // But per-sample update with smoothing is safer for artifacts, though
  // expensive. For simplicity here, we recompute (or ideally use a smoothed
  // variable). To save CPU, we'll assume coefficients don't change drastically
  // per sample or accept the cost. Optimization: Coeffs only depend on 'gainDb'
  // which depends on 'drive'.

  double A = std::pow(10.0, gainDb / 40.0);
  double w0 = 2.0 * juce::MathConstants<double>::pi *
              VT2RConstants::kPreEmphasisFreq / currentSampleRate;
  double alpha = std::sin(w0) / (2.0 * VT2RConstants::kPreEmphasisQ);

  double b0 = 1.0 + alpha * A;
  double b1 = -2.0 * std::cos(w0);
  double b2 = 1.0 - alpha * A;
  double a0 = 1.0 + alpha / A;
  double a1 = -2.0 * std::cos(w0);
  double a2 = 1.0 - alpha / A;

  // Normalize by a0
  float fb0 = float(b0 / a0);
  float fb1 = float(b1 / a0);
  float fb2 = float(b2 / a0);
  float fa1 = float(a1 / a0);
  float fa2 = float(a2 / a0);

  // Biquad Process
  float output = fb0 * input + fb1 * state.z1 + fb2 * state.z2 -
                 fa1 * state.z1 - fa2 * state.z2;

  // Update state (using DF1)
  // Actually standard DF1: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] -
  // a2*y[n-2] My state struct needs to store inputs and outputs or use DF2.
  // Let's us DF2 Transposed or DF1.
  // DF1 requires storing x[n-1], x[n-2], y[n-1], y[n-2].
  // Let's switch to DF2 for 2 state vars.
  // DF2:
  // w[n] = x[n] - a1*w[n-1] - a2*w[n-2]
  // y[n] = b0*w[n] + b1*w[n-1] + b2*w[n-2]

  // Let's use simpler state update for DF1 if I change struct, but I have z1,
  // z2. Let's assume z1, z2 are x_n1, x_n2? No usually y too for IIR. Let's
  // restart logic with a proper DF2.

  float w = input - fa1 * state.z1 - fa2 * state.z2;
  output = fb0 * w + fb1 * state.z1 + fb2 * state.z2;

  // Denormal protection
  if (std::abs(w) < 1e-20f)
    w = 0.0f;

  state.z2 = state.z1;
  state.z1 = w;

  return output;
}

float VT2BBlackProcessor::processSaturation(float input, float drive) {
  // Solid State / Transformer Hybird
  // Steep sigmoid: tanh(k * x)

  float normDrive = drive / 100.0f;

  // Input Gain boost: Up to +18dB driving the saturator
  float inputGain = 1.0f + normDrive * 8.0f;

  float x = input * inputGain;

  // Saturator
  // Tanh is clean odd harmonics.
  // To make it "aggressive/biting", we can use a harder clipper or scale tanh.
  // Or mix in some x^3.

  // Simple tanh for now, it's punchy if driven hard.
  return std::tanh(x);
}

float VT2BBlackProcessor::calculateMakeupGain(float drive) {
  float normDrive = drive / 100.0f;
  // Tanh limits to 1.0. If we boost input by 8x, we need to bring it down,
  // but not fully, to keep perceived loudness.
  // Auto-gain roughly compensates for the inputGain boost.
  // inputGain was 1 + drive*8.
  return 1.0f / (1.0f + normDrive * 4.0f); // Compensate partially
}

//==============================================================================
bool VT2BBlackProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *VT2BBlackProcessor::createEditor() {
  return new VT2BBlackEditor(*this);
}

//==============================================================================
void VT2BBlackProcessor::getStateInformation(juce::MemoryBlock &destData) {
  auto state = parameters.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void VT2BBlackProcessor::setStateInformation(const void *data,
                                             int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));

  if (xmlState.get() != nullptr)
    if (xmlState->hasTagName(parameters.state.getType()))
      parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new VT2BBlackProcessor();
}

/*
  ==============================================================================
    VT-2B Black - EMU AUDIO
    Console Bus Glue Processor

    "音を一つにする回路"
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
/**
 * VT-2B Black Processor
 *
 * コンソールサミング/バス回路を意識した密度増加型サチュレーション。
 * 派手さを抑え、音をまとめる方向に作用する。
 */
class VT2BBlackProcessor : public juce::AudioProcessor {
public:
  //==============================================================================
  VT2BBlackProcessor();
  ~VT2BBlackProcessor() override;

  //==============================================================================
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  //==============================================================================
  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  //==============================================================================
  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  //==============================================================================
  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  //==============================================================================
  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  //==============================================================================
  // パラメータアクセス
  juce::AudioProcessorValueTreeState &getParameters() { return parameters; }

private:
  //==============================================================================
  // パラメータ
  juce::AudioProcessorValueTreeState parameters;

  std::atomic<float> *driveParameter = nullptr;
  std::atomic<float> *mixParameter = nullptr;

  //==============================================================================
  // DSP状態
  double currentSampleRate = 44100.0;

  // Mid Boost Filter States (Biquad Direct Form I)
  struct FilterState {
      float z1 = 0.0f;
      float z2 = 0.0f;
  };
  FilterState midBoostStateL, midBoostStateR;
  
  // Previous input for basic high-pass/low-pass if needed
  float lastInL = 0.0f;
  float lastInR = 0.0f;

  // スムージング
  juce::SmoothedValue<float> smoothedDrive;
  juce::SmoothedValue<float> smoothedMix;

  //==============================================================================
  // DSP処理関数

  /**
   * VT-2R Saturation Model
   * Transformer + Solid State (Steep Sigmoid)
   */
  float processSaturation(float input, float drive);

  /**
   * Mid Frequency Emphasis (1kHz - 3kHz)
   * Boosts mids before saturation to create "Forward" character
   */
  float processPreEmphasis(float input, float drive, FilterState& state);

  /**
   * Automatic Makeup Gain
   */
  float calculateMakeupGain(float drive);

  //==============================================================================
  // パラメータレイアウト作成
  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VT2BBlackProcessor)
};

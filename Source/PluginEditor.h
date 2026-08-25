#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

/** A rotary slider with a caption underneath — the plugin's whole UI is four of these plus a mode switch. */
class LabelledKnob : public juce::Component
{
public:
    LabelledKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& caption)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 20);
        slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff5ec8c0));
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3f47));
        slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (slider);

        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (20));
        slider.setBounds (area);
    }

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class JJBreezeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit JJBreezeAudioProcessorEditor (JJBreezeAudioProcessor&);
    ~JJBreezeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    JJBreezeAudioProcessor& processorRef;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label slapSectionLabel;
    juce::Label vibratoSectionLabel;

    LabelledKnob pitchLKnob, pitchRKnob, delayLKnob, delayRKnob, focusKnob, mixKnob;
    LabelledKnob slapTimeKnob, slapFeedbackKnob, slapMixKnob;
    LabelledKnob vibratoRateKnob, vibratoDepthKnob, vibratoMixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JJBreezeAudioProcessorEditor)
};

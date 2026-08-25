cmake_minimum_required(VERSION 3.22)

project(jj-breeze VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(jj_breeze
    COMPANY_NAME "Gerov"
    BUNDLE_ID "com.gerov.jjbreeze"
    PLUGIN_MANUFACTURER_CODE Grov
    PLUGIN_CODE Jjbz
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "jj-breeze"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES "Fx" "Modulation"
    AU_EXPORT_PREFIX JJBreezeAU
)

target_sources(jj_breeze PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# The small circular mark drawn subtly in the editor header — cropped from
# images/47-warmth-icon.png (that file also bakes in "J.J.BREEZE" text,
# redundant next to the header's own title label, hence the separate crop).
juce_add_binary_data(jj_breeze_BinaryData
    SOURCES
        images/jjbreeze-mark.png
)

target_compile_definitions(jj_breeze PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(jj_breeze
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        jj_breeze_BinaryData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

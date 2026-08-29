cmake_minimum_required(VERSION 3.22)

project(jj-breeze VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version")

# Off in CI (see .github/workflows/build-windows.yml): the post-build copy
# targets system plugin folders (e.g. C:\Program Files\Common Files\VST3 on
# Windows) that need elevation a CI runner doesn't have, which would fail
# the whole build. On for normal local dev (scripts/install-local.sh also
# does this copy explicitly itself, redundantly but harmlessly).
option(JJ_BREEZE_COPY_PLUGIN_AFTER_BUILD "Copy the built plugin into the system plugin folders after building" ON)

# The finish the panel ships in - the one it wears until the user turns the
# finish selector in its header, which then remembers their choice per
# machine. See GearPalette::allThemes() in Source/GearTheme.h for the
# available ids ("green", "slate").
set(JJ_BREEZE_DEFAULT_THEME "green" CACHE STRING "Default finish id - see GearPalette::allThemes() in Source/GearTheme.h")

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
    PRODUCT_NAME "j.j.breeze"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${JJ_BREEZE_COPY_PLUGIN_AFTER_BUILD}
    VST3_CATEGORIES "Fx" "Modulation"
    AU_EXPORT_PREFIX JJBreezeAU
)

target_sources(jj_breeze PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)


target_compile_definitions(jj_breeze PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JJ_BREEZE_DEFAULT_THEME="${JJ_BREEZE_DEFAULT_THEME}"
)

target_link_libraries(jj_breeze
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

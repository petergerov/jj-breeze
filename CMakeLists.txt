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

# AAX (Pro Tools) is built only when Avid's SDK is at hand. That SDK may not
# be redistributed, so its path is handed in from outside rather than assumed:
# scripts/aax-sdk.sh (aax-sdk.ps1 on Windows) unpacks AAX/aax-sdk-*.zip and
# prints the path the dist scripts pass in here. Left empty — the default, and
# what a checkout without the SDK gets — the build produces AU, VST3 and
# Standalone exactly as before.
set(JJ_BREEZE_AAX_SDK_PATH "" CACHE PATH "Path to an unpacked AAX SDK; empty builds no AAX (see scripts/aax-sdk.sh)")

set(JJ_BREEZE_FORMATS AU VST3 Standalone)

if(JJ_BREEZE_AAX_SDK_PATH)
    juce_set_aax_sdk_path("${JJ_BREEZE_AAX_SDK_PATH}")
    list(APPEND JJ_BREEZE_FORMATS AAX)

    # JUCE would otherwise copy the built .aaxplugin into the machine-wide
    # Avid plug-ins folder after every build, the way it does for AU and VST3.
    # Unlike those, that folder is not writable without elevation, so the copy
    # would fail the build for everyone who has the SDK. Clearing the property
    # (JUCE reads it during juce_add_plugin below, so it has to happen here)
    # drops the step; the dist scripts and installers place the plugin instead.
    set_property(DIRECTORY PROPERTY JUCE_AAX_COPY_DIR "")

    message(STATUS "AAX SDK found, building the AAX format too: ${JJ_BREEZE_AAX_SDK_PATH}")
endif()

juce_add_plugin(jj_breeze
    COMPANY_NAME "Gerov"
    BUNDLE_ID "com.gerov.jjbreeze"
    PLUGIN_MANUFACTURER_CODE Grov
    PLUGIN_CODE Jjbz
    FORMATS ${JJ_BREEZE_FORMATS}
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

cmake_minimum_required(VERSION 3.22)

project(ACIDIZE VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: uses a local copy if ACIDIZE_JUCE_PATH is given, otherwise downloads it.
include(FetchContent)
if(ACIDIZE_JUCE_PATH)
    add_subdirectory(${ACIDIZE_JUCE_PATH} juce)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(ACIDIZE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND ACIDIZE_FORMATS AU)
endif()

juce_add_plugin(ACIDIZE
    COMPANY_NAME "REZONANZA"
    BUNDLE_ID com.rezonanza.acidize
    PLUGIN_MANUFACTURER_CODE Rznz
    PLUGIN_CODE Acdz
    FORMATS ${ACIDIZE_FORMATS}
    PRODUCT_NAME "ACIDIZE"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth)

juce_add_binary_data(ACIDIZE_Data
    SOURCES
        Resources/acidize_logo.png
        Resources/IBMPlexMono-Regular.ttf)

target_sources(ACIDIZE
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(ACIDIZE
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ACIDIZE
    PRIVATE
        ACIDIZE_Data
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

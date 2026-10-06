cmake_minimum_required(VERSION 3.22)

# Universal (Apple Silicon + Intel) y compatible con macOS 10.13+
# (estas lineas tienen que ir ANTES de project())
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)

project(VocodexPro VERSION 2.4.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VocodexPro
    COMPANY_NAME "Vocodex"
    BUNDLE_ID "com.vocodex.pro"
    PRODUCT_NAME "Vocodex Pro"
    PLUGIN_MANUFACTURER_CODE Vcdx
    PLUGIN_CODE Vcpr
    FORMATS VST3 AU
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(VocodexPro PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(VocodexPro PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0

target_link_libraries(VocodexPro
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags)

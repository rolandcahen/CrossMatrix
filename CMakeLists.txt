cmake_minimum_required(VERSION 3.22)

project(CrossMatrix VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)

add_subdirectory(JUCE)

juce_add_plugin(CrossMatrix
    COMPANY_NAME                "RolandCahen"
    COMPANY_CODE                "RCah"
    PLUGIN_MANUFACTURER_CODE    "RCah"
    PLUGIN_CODE                 "XMtx"
    BUNDLE_ID                   "fr.rolandcahen.crossmatrix"
    PRODUCT_NAME                "Cross Matrix"
    DESCRIPTION                 "Matrice de routage entrees/sorties avec snapshots automatisables"
    FORMATS                     VST3 AU
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Tools Spatial Surround
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     TRUE)

target_sources(CrossMatrix PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(CrossMatrix PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(CrossMatrix
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Test de routage : cmake --build build --target RoutingTest && ./build/RoutingTest_artefacts/Release/RoutingTest
juce_add_console_app(RoutingTest PRODUCT_NAME "RoutingTest")
target_sources(RoutingTest PRIVATE Tests/RoutingTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(RoutingTest PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JucePlugin_Name="Cross Matrix")
set_target_properties(RoutingTest PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_link_libraries(RoutingTest PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

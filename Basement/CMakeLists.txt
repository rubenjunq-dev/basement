cmake_minimum_required(VERSION 3.22)

# Doit être défini AVANT project() : cible macOS 10.13+ (Intel + Apple Silicon)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

project(Basement VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BASEMENT_BUILD_TESTS "Compile le banc de test DSP / captures d'écran" OFF)
option(BASEMENT_COPY_AFTER_BUILD "Copie le plugin dans le dossier VST3/AU système après compilation" OFF)

# ------------------------------------------------------------------ JUCE
# Pour utiliser une copie locale de JUCE (hors-ligne) :
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=/chemin/vers/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.16-security
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ------------------------------------------------------------------ Plugin
set(BASEMENT_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND BASEMENT_FORMATS AU)
endif()

juce_add_plugin(Basement
    COMPANY_NAME                "Basement Audio"
    BUNDLE_ID                   com.basementaudio.basement
    PLUGIN_MANUFACTURER_CODE    Bsmn
    PLUGIN_CODE                 Bsm1
    FORMATS                     ${BASEMENT_FORMATS}
    PRODUCT_NAME                "Basement"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    VST3_CATEGORIES             Fx
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     ${BASEMENT_COPY_AFTER_BUILD})

juce_generate_juce_header(Basement)

target_sources(Basement PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Basement PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Basement
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------ Banc de test (optionnel)
if(BASEMENT_BUILD_TESTS)
    juce_add_console_app(BasementTests PRODUCT_NAME "BasementTests")
    juce_generate_juce_header(BasementTests)

    target_sources(BasementTests PRIVATE
        Tests/TestMain.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(BasementTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_MP3AUDIOFORMAT=0
        JucePlugin_Name="Basement")

    target_link_libraries(BasementTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_recommended_config_flags)
endif()

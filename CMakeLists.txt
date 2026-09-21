cmake_minimum_required(VERSION 3.22)

project("Metaphysical Factory" VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Resolve JUCE (Bypasses broken distro package by using local clone or FetchContent)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif(EXISTS "/home/Atlin/JUCE/CMakeLists.txt")
    add_subdirectory("/home/Atlin/JUCE" JUCE)
else()
    include(FetchContent)
    message(STATUS "Fetching clean JUCE framework from official repository...")
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# 2. Declare Plugin Target
juce_add_plugin(MetaphysicalFactory
    COMPANY_NAME "Atlin"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Atln
    PLUGIN_CODE Mtfy
    FORMATS VST3 Standalone
    PRODUCT_NAME "Metaphysical Factory"
)

# 3. Source Files Registration
target_sources(MetaphysicalFactory PRIVATE
    # Processor & Editor
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp

    # DSP Architecture
    Source/DSP/OscillatorBank.h
    Source/DSP/Resochord.h
    Source/DSP/FilterRingMod.h
    Source/DSP/SampleLooper.h
    Source/DSP/SpinDelay.h
    Source/DSP/SpaceReverb.h
    Source/DSP/MotionEngine.h

    # UI & Aesthetics
    Source/UI/LissajousVisualizer.h
    Source/UI/MotionSlider.h
    Source/UI/MetaphysicalLookAndFeel.h
)

# 4. Include Paths
target_include_directories(MetaphysicalFactory PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI"
)

# 5. Link JUCE Modules
target_link_libraries(MetaphysicalFactory PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_audio_basics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics

    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# 6. Target Definitions
target_compile_definitions(MetaphysicalFactory PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

juce_generate_juce_header(MetaphysicalFactory)

cmake_minimum_required(VERSION 3.22)
project(ChopForge VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(CHOPFORGE_BUILD_TESTS "Build the engine test executable" ON)
option(CHOPFORGE_COPY_PLUGIN "Copy the built plugin into the user plugin folder" OFF)
set(CHOPFORGE_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE" CACHE PATH "Path to a JUCE checkout (fetched automatically if absent)")

if (EXISTS "${CHOPFORGE_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${CHOPFORGE_JUCE_DIR}" juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------- plugin
juce_add_plugin(ChopForge
    COMPANY_NAME                "Bajart Audio"
    BUNDLE_ID                   "com.bajartaudio.chopforge"
    PLUGIN_MANUFACTURER_CODE    Bjrt
    PLUGIN_CODE                 Chpf
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "ChopForge"
    DESCRIPTION                 "Creative audio chopper, slicer and re-sequencer"
    VST3_CATEGORIES             Fx Tools
    AU_MAIN_TYPE                kAudioUnitType_MusicEffect
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     ${CHOPFORGE_COPY_PLUGIN})

juce_generate_juce_header(ChopForge)

target_sources(ChopForge PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/WaveformView.cpp
    Source/UI/StepGrid.cpp)

target_include_directories(ChopForge PRIVATE Source)

target_compile_definitions(ChopForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_ALSA=0
    JUCE_JACK=0
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=0
    JUCE_USE_MP3AUDIOFORMAT=0
    JUCE_USE_WINDOWS_MEDIA_FORMAT=0)

target_link_libraries(ChopForge PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- tests
if (CHOPFORGE_BUILD_TESTS)
    juce_add_console_app(ChopForgeTests PRODUCT_NAME "ChopForgeTests")
    target_sources(ChopForgeTests PRIVATE Tests/EngineTests.cpp)
    target_include_directories(ChopForgeTests PRIVATE Source)
    target_compile_definitions(ChopForgeTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_FLAC=1 JUCE_USE_OGGVORBIS=0 JUCE_USE_MP3AUDIOFORMAT=0
        JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(ChopForgeTests PRIVATE
        juce::juce_audio_basics juce::juce_audio_formats juce::juce_dsp
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    enable_testing()
    add_test(NAME EngineTests COMMAND ChopForgeTests)
endif()

# ---------------------------------------------------------------- host test (drives the real processor)
if (CHOPFORGE_BUILD_TESTS)
    # A plain executable linked against the plugin's shared code (not juce_add_console_app,
    # which would redefine JUCE_STANDALONE_APPLICATION).
    add_executable(ChopForgeHostTests Tests/HostTests.cpp)
    target_include_directories(ChopForgeHostTests PRIVATE Source)
    target_compile_definitions(ChopForgeHostTests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
    target_link_libraries(ChopForgeHostTests PRIVATE
        ChopForge
        juce::juce_audio_processors juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp juce::juce_gui_extra
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME HostTests COMMAND ChopForgeHostTests)
endif()

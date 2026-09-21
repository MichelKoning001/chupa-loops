cmake_minimum_required(VERSION 3.24)

# must be set before project()
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

# every GitHub build gets its own version number (hosts such as Logic cache plug-ins by version)
set(SLICETRIBE_BUILD_NUMBER 0)
if(DEFINED ENV{GITHUB_RUN_NUMBER})
    set(SLICETRIBE_BUILD_NUMBER $ENV{GITHUB_RUN_NUMBER})
endif()

# hosts pack the version as major.minor.patch with minor and patch below 256, so the build number
# is spread over both: build 1..255 = 1.1.x, build 256 = 1.2.0, ...
math (EXPR CHUPALOOPS_MINOR "1 + ${SLICETRIBE_BUILD_NUMBER} / 256")
math (EXPR CHUPALOOPS_PATCH "${SLICETRIBE_BUILD_NUMBER} % 256")
project(ChupaLoops VERSION 1.${CHUPALOOPS_MINOR}.${CHUPALOOPS_PATCH} LANGUAGES C CXX)

# Windows: link the C++ runtime statically, so no Visual C++ redistributable is needed
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Libraries (downloaded automatically)
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    ON)

FetchContent_Declare(signalsmith-linear
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
    GIT_TAG        0.3.1
    GIT_SHALLOW    ON)

FetchContent_Declare(signalsmith-stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG        57b93f4e9206a089a45387eaa39bdc9f310d3308)

FetchContent_MakeAvailable(JUCE signalsmith-linear signalsmith-stretch)

# treat the stretch library headers as system headers (no warnings from third-party code)
foreach(_lib signalsmith-linear signalsmith-stretch)
    get_target_property(_inc ${_lib} INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(${_lib} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_inc}")
endforeach()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
set(SLICETRIBE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SLICETRIBE_FORMATS AU)
endif()

juce_add_plugin(ChupaLoops
    COMPANY_NAME                "Percep-tion"
    COMPANY_WEBSITE             "https://thebeginningofhouse.nl"
    BUNDLE_ID                   "com.perception.chupaloops"
    PLUGIN_MANUFACTURER_CODE    Prcp
    PLUGIN_CODE                 Chup
    PRODUCT_NAME                "Chupa Loops"
    DESCRIPTION                 "Loop slicer & loop generator"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Sampler
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    FORMATS                     ${SLICETRIBE_FORMATS}
    ICON_BIG                    Resources/icon.png
    COPY_PLUGIN_AFTER_BUILD     FALSE)

juce_generate_juce_header(ChupaLoops)

juce_add_binary_data(SliceTribeData
    HEADER_NAME SliceTribeData.h
    NAMESPACE SliceTribeData
    SOURCES
        Resources/fonts/Inter-Regular.ttf
        Resources/fonts/Inter-SemiBold.ttf
        Resources/fonts/Inter-Bold.ttf
        Resources/fonts/EricaOne-Regular.ttf
        Resources/fonts/Boldonse-Regular.ttf
        Resources/fonts/Tektur-Medium.ttf
        Resources/fonts/BigShoulders-Bold.ttf)

set(CHUPALOOPS_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SliceEngine.cpp
    Source/Components.cpp
    Source/Skins.cpp
    Source/Presets.cpp
    Source/FactoryPresets.cpp
    Source/Overlays.cpp
    Source/Fx.cpp)

# GitHub repository used by the update check in the About screen (set by the build workflow)
set(CHUPALOOPS_UPDATE_REPO "" CACHE STRING "owner/repo for the update check")

target_sources(ChupaLoops PRIVATE ${CHUPALOOPS_SOURCES})

target_include_directories(ChupaLoops PRIVATE Source)

target_compile_definitions(ChupaLoops PUBLIC
    # Signalsmith pulls in Apple's Accelerate (and with it Carbon's Point/Component) on macOS.
    # JUCE's global "using namespace juce" would make those names ambiguous, so we keep it off
    # and qualify juce names ourselves.
    DONT_SET_USING_JUCE_NAMESPACE=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_USE_MP3AUDIOFORMAT=1
    CHUPALOOPS_UPDATE_REPO="${CHUPALOOPS_UPDATE_REPO}"
    CHUPALOOPS_BUILD_NUMBER=${SLICETRIBE_BUILD_NUMBER})

if(UNIX AND NOT APPLE)
    # Lean Linux build (used for automated testing only)
    target_compile_definitions(ChupaLoops PUBLIC
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_ALSA=0 JUCE_JACK=0)
endif()

target_link_libraries(ChupaLoops
    PRIVATE
        SliceTribeData
        juce::juce_audio_utils
        juce::juce_audio_formats
        signalsmith-stretch
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # exact float compares are intended (change detection); per-file options come after JUCE's warning flags
    set_source_files_properties(${CHUPALOOPS_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wno-float-equal")
endif()
if(MSVC)
    target_compile_options(ChupaLoops PRIVATE /bigobj /utf-8)
endif()

# ---------------------------------------------------------------------------
# Offline test tool (renders example loops to WAV) – optional
# ---------------------------------------------------------------------------
option(SLICETRIBE_BUILD_TESTS "Build offline render test tool" OFF)
if(SLICETRIBE_BUILD_TESTS)
    juce_add_console_app(RenderTest PRODUCT_NAME "RenderTest")
    juce_generate_juce_header(RenderTest)
    target_sources(RenderTest PRIVATE tests/RenderTest.cpp Source/SliceEngine.cpp)
    target_include_directories(RenderTest PRIVATE Source)
    target_compile_definitions(RenderTest PRIVATE DONT_SET_USING_JUCE_NAMESPACE=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if(MSVC)
        target_compile_options(RenderTest PRIVATE /bigobj /utf-8)
    endif()
    target_link_libraries(RenderTest PRIVATE
        juce::juce_audio_formats juce::juce_audio_basics signalsmith-stretch
        juce::juce_recommended_config_flags)

    juce_add_console_app(UITest PRODUCT_NAME "UITest")
    juce_generate_juce_header(UITest)
    target_sources(UITest PRIVATE tests/UITest.cpp ${CHUPALOOPS_SOURCES})
    target_include_directories(UITest PRIVATE Source)
    if(MSVC)
        target_compile_options(UITest PRIVATE /bigobj /utf-8)
    endif()
    target_compile_definitions(UITest PRIVATE DONT_SET_USING_JUCE_NAMESPACE=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_USE_MP3AUDIOFORMAT=1
        JucePlugin_VersionString="${PROJECT_VERSION}" CHUPALOOPS_BUILD_NUMBER=${SLICETRIBE_BUILD_NUMBER}
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_ALSA=0 JUCE_JACK=0)
    target_link_libraries(UITest PRIVATE SliceTribeData juce::juce_audio_utils juce::juce_audio_processors signalsmith-stretch
        juce::juce_recommended_config_flags)
endif()

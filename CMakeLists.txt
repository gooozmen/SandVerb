cmake_minimum_required(VERSION 3.22)

# macOS: универсальный бинарник (Apple Silicon + Intel). Задаётся до project().
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
endif()

project(SandVerb VERSION 0.1.1)

# Windows: статический рантайм, чтобы плагин работал без установки Visual C++ Redistributable
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Копировать собранный VST3 в системную папку плагинов (для разработки). В CI выключается.
option(SANDVERB_COPY_PLUGIN "Copy the plugin to the system plugin folder after build" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SandVerb
    COMPANY_NAME "gmuse"
    PLUGIN_MANUFACTURER_CODE Yrnm
    PLUGIN_CODE Sdvb
    FORMATS VST3 Standalone
    PRODUCT_NAME "SandVerb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD ${SANDVERB_COPY_PLUGIN})

target_sources(SandVerb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/FlowGraph.cpp
    Source/Inspector.cpp)

target_compile_definitions(SandVerb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SandVerb
    PRIVATE juce::juce_audio_utils
    PUBLIC  juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

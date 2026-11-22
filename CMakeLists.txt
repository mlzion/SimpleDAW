cmake_minimum_required(VERSION 3.22)
project(SimpleDAW VERSION 0.23.1 LANGUAGES C CXX)

include(FetchContent)
# Use the JUCE revision recorded by Tracktion Engine v3.2.0.
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 19edd538429c93d277bf95b55aaa7e3eb545f951)
FetchContent_Declare(tracktion
    GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
    GIT_TAG 0a5f4e6a5f53d09c89b414a44386a12df7fa1ec6 # v3.2.0
    GIT_SUBMODULES ""
    SOURCE_SUBDIR modules)
FetchContent_MakeAvailable(juce tracktion)
include(cmake/TracktionAudioOverlap.cmake)

set(SIMPLEDAW_ASIO_SDK "" CACHE PATH "Optional directory containing iasiodrv.h")

# Use major.minor for a zero patch release and keep nonzero patch or tweak numbers.
set(SIMPLEDAW_RELEASE_VERSION "${PROJECT_VERSION}")
if(PROJECT_VERSION STREQUAL "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.0")
    set(SIMPLEDAW_RELEASE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
endif()

juce_add_gui_app(SimpleDAW PRODUCT_NAME "SimpleDAW" VERSION "${PROJECT_VERSION}")
if(WIN32)
    set_target_properties(SimpleDAW PROPERTIES OUTPUT_NAME "SimpleDAW-v${SIMPLEDAW_RELEASE_VERSION}")
    # Regenerate JUCE's version resource when its input changes in an existing build.
    get_target_property(juce_sources SimpleDAW JUCE_GENERATED_SOURCES_DIRECTORY)
    get_target_property(juce_info SimpleDAW JUCE_INFO_FILE)
    add_custom_command(OUTPUT "${juce_sources}/SimpleDAW_resources.rc" APPEND DEPENDS "${juce_info}")

    string(CONFIGURE [=[
{
  "version": "@PROJECT_VERSION@",
  "windowTitle": "SimpleDAW v@SIMPLEDAW_RELEASE_VERSION@",
  "executable": "$<TARGET_FILE:SimpleDAW>",
  "artifactName": "SimpleDAW-v@SIMPLEDAW_RELEASE_VERSION@-Windows-x64"
}
]=] release_info @ONLY)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/SimpleDAW-$<CONFIG>.json" CONTENT "${release_info}")
endif()
if(MSVC)
    option(SIMPLEDAW_MSVC_DYNAMIC_RUNTIME "Use the standard DLL-based MSVC runtime (/MD). Disable only for /MT diagnostics." ON)
    if(SIMPLEDAW_MSVC_DYNAMIC_RUNTIME)
        set_property(TARGET SimpleDAW PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else()
        set_property(TARGET SimpleDAW PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()
juce_generate_juce_header(SimpleDAW)
target_sources(SimpleDAW PRIVATE src/Main.cpp tests/EffectsFoundation.cpp)
target_compile_features(SimpleDAW PRIVATE cxx_std_20)
target_include_directories(SimpleDAW PRIVATE src)
target_compile_definitions(SimpleDAW PRIVATE
    SIMPLEDAW_RELEASE_VERSION="${SIMPLEDAW_RELEASE_VERSION}"
    JUCE_PLUGINHOST_VST3=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1 TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1)
if(MSVC)
    target_compile_options(SimpleDAW PRIVATE /utf-8)
endif()
target_link_libraries(SimpleDAW PRIVATE tracktion::tracktion_engine juce::juce_audio_utils)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(SimpleDAW PRIVATE atomic)
endif()
if(SIMPLEDAW_ASIO_SDK)
    if(NOT WIN32 OR NOT EXISTS "${SIMPLEDAW_ASIO_SDK}/iasiodrv.h")
        message(FATAL_ERROR "ASIO requires Windows and a directory containing iasiodrv.h")
    endif()
    target_compile_definitions(SimpleDAW PRIVATE JUCE_ASIO=1)
    target_include_directories(SimpleDAW PRIVATE "${SIMPLEDAW_ASIO_SDK}")
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME project_round_trip COMMAND SimpleDAW --smoke-test)
    add_test(NAME main_gui COMMAND SimpleDAW --gui-test)
    add_test(NAME gui_corrections COMMAND SimpleDAW --correction-test)
    add_test(NAME universal_actions COMMAND SimpleDAW --uat-test)
    add_test(NAME effects_foundation COMMAND SimpleDAW --effects-foundation-test)
    add_test(NAME midi_import COMMAND SimpleDAW --midi-import-test)
    add_test(NAME midi_inspector COMMAND SimpleDAW --midi-inspector-test)
    add_test(NAME inspector_loop COMMAND SimpleDAW --inspector-loop-test)
    add_test(NAME inspector_clone COMMAND SimpleDAW --inspector-clone-test)
    add_test(NAME inspector_effects COMMAND SimpleDAW --inspector-effects-test)
    add_test(NAME audio_overlap COMMAND SimpleDAW --audio-overlap-test)
    add_test(NAME effects_editor COMMAND SimpleDAW --effects-editor-test)
    set_tests_properties(project_round_trip main_gui gui_corrections universal_actions
        effects_foundation effects_editor midi_import midi_inspector inspector_loop inspector_clone
        inspector_effects audio_overlap
        PROPERTIES TIMEOUT 60)
endif()

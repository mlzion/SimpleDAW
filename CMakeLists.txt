cmake_minimum_required(VERSION 3.22)
project(SimpleDAW VERSION 0.21.0 LANGUAGES C CXX)

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

set(SIMPLEDAW_ASIO_SDK "" CACHE PATH "Optional directory containing iasiodrv.h")

juce_add_gui_app(SimpleDAW PRODUCT_NAME "SimpleDAW")
if(MSVC)
    option(SIMPLEDAW_MSVC_DYNAMIC_RUNTIME "Use the DLL-based MSVC runtime (/MD)" ON)
    if(SIMPLEDAW_MSVC_DYNAMIC_RUNTIME)
        set_property(TARGET SimpleDAW PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else()
        set_property(TARGET SimpleDAW PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()
juce_generate_juce_header(SimpleDAW)
target_sources(SimpleDAW PRIVATE src/Main.cpp)
target_compile_features(SimpleDAW PRIVATE cxx_std_20)
target_compile_definitions(SimpleDAW PRIVATE
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
    set_tests_properties(project_round_trip main_gui gui_corrections PROPERTIES TIMEOUT 60)
endif()

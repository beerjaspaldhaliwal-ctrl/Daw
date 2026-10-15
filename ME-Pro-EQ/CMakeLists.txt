cmake_minimum_required(VERSION 3.22)

project(MEProEQ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MEPROEQ_BUILD_PLUGIN "Build the VST3 plugin (requires JUCE)" ON)
option(MEPROEQ_BUILD_TESTS "Build the automated test suites" ON)
option(MEPROEQ_BUILD_BENCHMARKS "Build the CPU benchmark tool" ON)
set(MEPROEQ_JUCE_DIR "" CACHE PATH "Use a local JUCE checkout instead of downloading JUCE 9.0.4")

if(MSVC)
    add_compile_options(/utf-8 /permissive- /Zc:__cplusplus /W4 /wd4100)
    # The DSP relies on IEEE semantics (NaN checks, exact identity paths): never use /fp:fast.
    add_compile_options(/fp:precise)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

#-------------------------------------------------------------------------------
# Framework-independent DSP core
#-------------------------------------------------------------------------------
add_library(meproeq_dsp STATIC
    Source/DSP/FilterDesign.cpp
    Source/DSP/FilterBand.cpp
    Source/DSP/EQEngine.cpp
)
target_include_directories(meproeq_dsp PUBLIC Source)
target_compile_features(meproeq_dsp PUBLIC cxx_std_20)

#-------------------------------------------------------------------------------
# Tests
#-------------------------------------------------------------------------------
if(MEPROEQ_BUILD_TESTS)
    enable_testing()
    add_executable(meproeq_dsp_tests
        Tests/TestMain.cpp
        Tests/FilterTests.cpp
        Tests/EngineTests.cpp
        Tests/DynamicEQTests.cpp
        Tests/AutomationTests.cpp
    )
    target_link_libraries(meproeq_dsp_tests PRIVATE meproeq_dsp)
    target_include_directories(meproeq_dsp_tests PRIVATE Tests)
    add_test(NAME dsp_tests COMMAND meproeq_dsp_tests --report ${CMAKE_BINARY_DIR}/reports/dsp_test_report.md)
endif()

#-------------------------------------------------------------------------------
# Plugin (JUCE)
#-------------------------------------------------------------------------------
if(MEPROEQ_BUILD_PLUGIN)
    include(cmake/JUCE.cmake)
    include(cmake/Plugin.cmake)
endif()

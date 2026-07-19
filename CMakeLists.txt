cmake_minimum_required(VERSION 3.25)

project(Fishpond VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FishpondDependencies)
option(FISHPOND_BUILD_JUCE_FIXTURES "Build JUCE VST3 test fixtures" OFF)
option(FISHPOND_BUILD_APP "Build the Fishpond JUCE desktop shell" OFF)
option(FISHPOND_BUILD_APP "Build the Fishpond JUCE desktop application" OFF)

add_library(fishpond_build_config INTERFACE)
target_compile_features(fishpond_build_config INTERFACE cxx_std_17)
target_compile_definitions(fishpond_build_config INTERFACE
    FISHPOND_VERSION="${PROJECT_VERSION}"
)

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_subdirectory(tests)
endif()

if(FISHPOND_BUILD_JUCE_FIXTURES)
    fishpond_require_juce()
    add_subdirectory(tests/fixtures/juce)
endif()

if(FISHPOND_BUILD_APP)
    fishpond_require_juce()
    add_subdirectory(app)
endif()

if(FISHPOND_BUILD_APP)
    fishpond_require_juce()
    add_subdirectory(app)
endif()

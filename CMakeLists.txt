cmake_minimum_required(VERSION 3.25)

project(Fishpond VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FishpondDependencies)

add_library(fishpond_build_config INTERFACE)
target_compile_features(fishpond_build_config INTERFACE cxx_std_17)
target_compile_definitions(fishpond_build_config INTERFACE
    FISHPOND_VERSION="${PROJECT_VERSION}"
)

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_subdirectory(tests)
endif()

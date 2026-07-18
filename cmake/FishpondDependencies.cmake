include_guard(GLOBAL)

# Pinned framework provenance. This skeleton does not fetch dependencies by
# default, so its configure/build smoke test is deterministic and offline.
set(FISHPOND_JUCE_VERSION "8.0.13" CACHE STRING "Approved JUCE release tag")
set(FISHPOND_CPYTHON_VERSION "3.12.13" CACHE STRING "Approved embedded CPython release")
set(FISHPOND_JUCE_SOURCE_DIR "" CACHE PATH "Optional local JUCE source tree")
option(FISHPOND_FETCH_JUCE "Fetch the pinned JUCE source when a host target needs it" OFF)

function(fishpond_require_juce)
    if(TARGET juce::juce_audio_basics)
        return()
    endif()

    if(FISHPOND_JUCE_SOURCE_DIR)
        add_subdirectory("${FISHPOND_JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/juce-build" EXCLUDE_FROM_ALL)
        return()
    endif()

    if(NOT FISHPOND_FETCH_JUCE)
        message(FATAL_ERROR
            "JUCE is required by this target. Set FISHPOND_JUCE_SOURCE_DIR to a verified JUCE ${FISHPOND_JUCE_VERSION} checkout or set FISHPOND_FETCH_JUCE=ON.")
    endif()

    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${FISHPOND_JUCE_VERSION}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(juce)
endfunction()

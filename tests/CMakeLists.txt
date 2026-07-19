add_executable(fishpond_p0_build_contract unit/build_contract.cpp)
target_link_libraries(fishpond_p0_build_contract PRIVATE fishpond_build_config)

add_test(NAME UT_BuildContract_Compiles COMMAND fishpond_p0_build_contract)
set_tests_properties(UT_BuildContract_Compiles PROPERTIES
    LABELS "UT;P0;BUILD-CONTRACT"
)

add_executable(fishpond_p0_foundations p0_foundations.cpp)
target_link_libraries(fishpond_p0_foundations PRIVATE fishpond_build_config)

add_executable(fishpond_p1_audio_shell integration/audio_shell.cpp ../src/AudioShell.cpp)
target_link_libraries(fishpond_p1_audio_shell PRIVATE fishpond_build_config)
target_include_directories(fishpond_p1_audio_shell PRIVATE "${PROJECT_SOURCE_DIR}/src")

set(FISHPOND_P1_AUDIO_SHELL_TESTS
    IT_AudioDeviceState
    IT_AudioEngineLifecycle
)
foreach(test_id IN LISTS FISHPOND_P1_AUDIO_SHELL_TESTS)
    add_test(NAME ${test_id} COMMAND fishpond_p1_audio_shell ${test_id})
    set_tests_properties(${test_id} PROPERTIES LABELS "IT;P1;FR-004;FR-050")
endforeach()

set(FISHPOND_P0_FOUNDATION_TESTS
    IT_FixtureInstrumentMidiAndAudio
    IT_FixtureEffectParameterAndStateFailures
    IT_FixtureScanLoadAndLayoutFailures
    RT_CallbackGuard
    RT_NoPythonOrUIInCallback
    IT_GraphCommandLifecycle
)
foreach(test_id IN LISTS FISHPOND_P0_FOUNDATION_TESTS)
    add_test(NAME ${test_id} COMMAND fishpond_p0_foundations ${test_id})
    set_tests_properties(${test_id} PROPERTIES LABELS "P0;FOUNDATIONS")
endforeach()

# These IDs are the approved P0.1 corpus. They are registered now to make the
# traceability contract executable, but remain disabled until the independently
# implemented runtime exists. P0.6 will reject disabled required tests at the
# phase gate; they are not counted as passing evidence.
set(FISHPOND_PENDING_SARDINE_COMPAT_TESTS
    UT_SardineCompat_PlayerAssignmentReplaces
    UT_SardineCompat_PatternNotesAndLiterals
    UT_SardineCompat_PeriodPattern
    UT_SardineCompat_QuantizedReplacement
    UT_SardineCompat_TargetNormalization
    UT_SardineCompat_TargetCollision
    UT_SardineCompat_InvalidValues
    IT_SardineCompat_UnknownTargetIsolation
    IT_SardineCompat_SilenceAndPanic
)

foreach(test_id IN LISTS FISHPOND_PENDING_SARDINE_COMPAT_TESTS)
    add_test(NAME ${test_id}
        COMMAND "${CMAKE_COMMAND}" -E false
    )
    set_tests_properties(${test_id} PROPERTIES
        DISABLED TRUE
        LABELS "P0;SARDINE-COMPAT;PENDING-RUNTIME"
    )
endforeach()

set(FISHPOND_CTEST_CONFIG_ARGUMENTS)
set(FISHPOND_TRACEABILITY_CONFIG_ARGUMENTS)
if(CMAKE_CONFIGURATION_TYPES)
    # Visual Studio/Xcode keep test metadata beneath the selected configuration.
    list(APPEND FISHPOND_CTEST_CONFIG_ARGUMENTS -C "$<CONFIG>")
    list(APPEND FISHPOND_TRACEABILITY_CONFIG_ARGUMENTS --configuration "$<CONFIG>")
endif()

add_custom_target(traceability-report
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${PROJECT_BINARY_DIR}" ${FISHPOND_CTEST_CONFIG_ARGUMENTS} --output-junit "${PROJECT_BINARY_DIR}/ctest-results.xml" -L "P0"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/export_traceability.py"
        --build-dir "${PROJECT_BINARY_DIR}"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/traceability-manifest.json"
        --output "${PROJECT_BINARY_DIR}/traceability.json"
        --results "${PROJECT_BINARY_DIR}/ctest-results.xml"
        ${FISHPOND_TRACEABILITY_CONFIG_ARGUMENTS}
    DEPENDS fishpond_p0_build_contract fishpond_p0_foundations
    VERBATIM
)

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

# The approved Sardine-compatible P1 corpus is deterministic and requires no
# audio device, plugin, network, or external Python package.
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
    add_test(NAME ${test_id} COMMAND fishpond_p1_runtime ${test_id})
    set_tests_properties(${test_id} PROPERTIES LABELS "P1;SARDINE-COMPAT")
endforeach()

add_executable(fishpond_p1_runtime
    runtime/sardine_compat.cpp
    "${PROJECT_SOURCE_DIR}/src/runtime/Runtime.cpp"
)
target_link_libraries(fishpond_p1_runtime PRIVATE fishpond_build_config)
target_include_directories(fishpond_p1_runtime PRIVATE "${PROJECT_SOURCE_DIR}/src")

if(TARGET fishpond_embedded_python)
    add_executable(fishpond_p1_embedded_python runtime/embedded_python.cpp)
    target_link_libraries(fishpond_p1_embedded_python PRIVATE fishpond_embedded_python)
    add_executable(fishpond_p1_python_worker runtime/python_execution_worker.cpp)
    target_link_libraries(fishpond_p1_python_worker PRIVATE fishpond_embedded_python)
    add_test(NAME IT_EmbeddedPythonPersistentNamespace COMMAND fishpond_p1_embedded_python persistent)
    add_test(NAME IT_EmbeddedPythonErrorIsolation COMMAND fishpond_p1_embedded_python error-isolation)
    add_test(NAME UT_EmbeddedPythonNamedPlayerReplacement COMMAND fishpond_p1_embedded_python player-replacement)
    add_test(NAME UT_EmbeddedPythonSilenceAndPanic COMMAND fishpond_p1_embedded_python silence-panic)
    add_test(NAME UT_EmbeddedPythonTargetRequired COMMAND fishpond_p1_embedded_python target-required)
    add_test(NAME IT_EmbeddedPythonSyntaxLocation COMMAND fishpond_p1_embedded_python syntax-location)
    add_test(NAME IT_PythonExecutionWorkerOffCallerThread COMMAND fishpond_p1_python_worker)
    set_tests_properties(IT_EmbeddedPythonPersistentNamespace IT_EmbeddedPythonErrorIsolation
        IT_EmbeddedPythonSyntaxLocation IT_PythonExecutionWorkerOffCallerThread
        PROPERTIES LABELS "IT;P1;FR-012;FR-013")
    set_tests_properties(UT_EmbeddedPythonNamedPlayerReplacement UT_EmbeddedPythonSilenceAndPanic
        UT_EmbeddedPythonTargetRequired PROPERTIES LABELS "UT;P1;FR-012;FR-013;SARDINE-COMPAT")
endif()

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

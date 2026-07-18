add_executable(fishpond_p0_build_contract unit/build_contract.cpp)
target_link_libraries(fishpond_p0_build_contract PRIVATE fishpond_build_config)

add_test(NAME UT_BuildContract_Compiles COMMAND fishpond_p0_build_contract)
set_tests_properties(UT_BuildContract_Compiles PROPERTIES
    LABELS "UT;P0;BUILD-CONTRACT"
)

add_executable(fishpond_p0_foundations p0_foundations.cpp)
target_link_libraries(fishpond_p0_foundations PRIVATE fishpond_build_config)

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

add_custom_target(traceability-report
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${PROJECT_BINARY_DIR}" --output-junit "${PROJECT_BINARY_DIR}/ctest-results.xml" -L "P0"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/export_traceability.py"
        --build-dir "${PROJECT_BINARY_DIR}"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/traceability-manifest.json"
        --output "${PROJECT_BINARY_DIR}/traceability.json"
        --results "${PROJECT_BINARY_DIR}/ctest-results.xml"
    DEPENDS fishpond_p0_build_contract fishpond_p0_foundations
    VERBATIM
)

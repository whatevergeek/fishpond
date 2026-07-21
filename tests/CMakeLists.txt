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
    UT_SardineCompat_ChordGroups
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
    "${PROJECT_SOURCE_DIR}/src/mixer/ChannelRegistry.cpp"
)
target_link_libraries(fishpond_p1_runtime PRIVATE fishpond_build_config)
target_include_directories(fishpond_p1_runtime PRIVATE "${PROJECT_SOURCE_DIR}/src")

add_executable(fishpond_p2_channels
    runtime/channel_registry.cpp
    "${PROJECT_SOURCE_DIR}/src/mixer/ChannelRegistry.cpp"
    "${PROJECT_SOURCE_DIR}/src/runtime/Runtime.cpp"
)
target_link_libraries(fishpond_p2_channels PRIVATE fishpond_build_config)
target_include_directories(fishpond_p2_channels PRIVATE "${PROJECT_SOURCE_DIR}/src")
foreach(test_id IN ITEMS UT_ChannelAliasDerivation UT_ChannelAliasCollision IT_ChannelRenamePreservesIdentity IT_ChannelTargetResolution IT_ChannelFourInstrumentRouting)
    if(test_id STREQUAL "UT_ChannelAliasDerivation")
        set(test_argument "aliases")
    elseif(test_id STREQUAL "UT_ChannelAliasCollision")
        set(test_argument "collisions")
    elseif(test_id STREQUAL "IT_ChannelTargetResolution")
        set(test_argument "target-routing")
    elseif(test_id STREQUAL "IT_ChannelFourInstrumentRouting")
        set(test_argument "four-instrument-routing")
    else()
        set(test_argument "rename")
    endif()
    add_test(NAME ${test_id} COMMAND fishpond_p2_channels ${test_argument})
    set_tests_properties(${test_id} PROPERTIES LABELS "P2;FR-034;FR-034a")
endforeach()

add_executable(fishpond_p1_event_boundary runtime/note_event_queue.cpp)
target_link_libraries(fishpond_p1_event_boundary PRIVATE fishpond_build_config)
target_include_directories(fishpond_p1_event_boundary PRIVATE "${PROJECT_SOURCE_DIR}/src")
set(FISHPOND_P1_EVENT_BOUNDARY_TESTS
    UT_NoteEventQueueCapacity
    IT_NoteEventQueueOrdering
    IT_NoteEventQueuePanic
    IT_NoteEventDispatchOrdering
    IT_NoteEventNoteOffReservation
    UT_NoteEventValidation
    IT_BassSchedulerDeviceTiming
    IT_BassSchedulerMultiplePlayers
    IT_BassSchedulerSinglePlayerStop
    IT_BassSchedulerRests
    IT_BassSchedulerChordGroups
    IT_BassSchedulerBarQuantization
    IT_P1ValidationProfile
)
foreach(test_id IN LISTS FISHPOND_P1_EVENT_BOUNDARY_TESTS)
    if(test_id STREQUAL "UT_NoteEventQueueCapacity")
        set(test_argument "capacity")
        set(test_labels "UT;P1;FR-052")
    elseif(test_id STREQUAL "IT_NoteEventQueueOrdering")
        set(test_argument "ordering")
        set(test_labels "IT;P1;FR-040;FR-052")
    elseif(test_id STREQUAL "IT_NoteEventDispatchOrdering")
        set(test_argument "dispatch")
        set(test_labels "IT;P1;FR-040;FR-052")
    elseif(test_id STREQUAL "IT_NoteEventNoteOffReservation")
        set(test_argument "note-reservation")
        set(test_labels "IT;P1;FR-040;FR-052")
    elseif(test_id STREQUAL "UT_NoteEventValidation")
        set(test_argument "note-validation")
        set(test_labels "UT;P1;FR-041;FR-052")
    elseif(test_id STREQUAL "IT_BassSchedulerDeviceTiming")
        set(test_argument "scheduler-timing")
        set(test_labels "IT;P1;FR-016;FR-040;FR-052")
    elseif(test_id STREQUAL "IT_BassSchedulerMultiplePlayers")
        set(test_argument "scheduler-multiple")
        set(test_labels "IT;P1;FR-012;FR-040;FR-052")
    elseif(test_id STREQUAL "IT_BassSchedulerSinglePlayerStop")
        set(test_argument "scheduler-remove")
        set(test_labels "IT;P1;FR-012;FR-018;FR-040;FR-052")
    elseif(test_id STREQUAL "IT_BassSchedulerRests")
        set(test_argument "scheduler-rests")
        set(test_labels "IT;P1;FR-015;FR-040;FR-045")
    elseif(test_id STREQUAL "IT_BassSchedulerChordGroups")
        set(test_argument "scheduler-chords")
        set(test_labels "IT;P2;FR-012;FR-040;SARDINE-COMPAT")
    elseif(test_id STREQUAL "IT_BassSchedulerBarQuantization")
        set(test_argument "scheduler-bar-quantization")
        set(test_labels "IT;P1;FR-012;FR-016;FR-040;SARDINE-COMPAT")
    elseif(test_id STREQUAL "IT_P1ValidationProfile")
        set(test_argument "scheduler-validation-profile")
        set(test_labels "IT;P1;FR-012;FR-040;FR-052;MVP-PROFILE")
    else()
        set(test_argument "panic")
        set(test_labels "IT;P1;FR-018;FR-052")
    endif()
    add_test(NAME ${test_id} COMMAND fishpond_p1_event_boundary ${test_argument})
    set_tests_properties(${test_id} PROPERTIES LABELS "${test_labels}")
endforeach()

add_executable(fishpond_p1_graph_handoff runtime/prepared_graph_handoff.cpp)
target_link_libraries(fishpond_p1_graph_handoff PRIVATE fishpond_build_config)
target_include_directories(fishpond_p1_graph_handoff PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME IT_PreparedGraphHandoff COMMAND fishpond_p1_graph_handoff command)
add_test(NAME IT_RetiredGraphReclaimedOffAudio COMMAND fishpond_p1_graph_handoff retire)
set_tests_properties(IT_PreparedGraphHandoff IT_RetiredGraphReclaimedOffAudio
    PROPERTIES LABELS "IT;P1;FR-027;FR-051;FR-053")

if(TARGET fishpond_embedded_python)
    add_executable(fishpond_p1_embedded_python runtime/embedded_python.cpp)
    target_link_libraries(fishpond_p1_embedded_python PRIVATE fishpond_embedded_python)
    add_executable(fishpond_p1_python_worker runtime/python_execution_worker.cpp)
    target_link_libraries(fishpond_p1_python_worker PRIVATE fishpond_embedded_python)
    add_test(NAME IT_EmbeddedPythonPersistentNamespace COMMAND fishpond_p1_embedded_python persistent)
    add_test(NAME IT_EmbeddedPythonErrorIsolation COMMAND fishpond_p1_embedded_python error-isolation)
    add_test(NAME UT_EmbeddedPythonNamedPlayerReplacement COMMAND fishpond_p1_embedded_python player-replacement)
    add_test(NAME UT_EmbeddedPythonSilenceAndPanic COMMAND fishpond_p1_embedded_python silence-panic)
    add_test(NAME UT_EmbeddedPythonSinglePlayerStop COMMAND fishpond_p1_embedded_python single-player-stop)
    add_test(NAME UT_EmbeddedPythonMultiplePlayerStop COMMAND fishpond_p1_embedded_python multiple-player-stop)
    add_test(NAME UT_EmbeddedPythonTargetRequired COMMAND fishpond_p1_embedded_python target-required)
    add_test(NAME UT_EmbeddedPythonClockState COMMAND fishpond_p1_embedded_python clock-state)
    add_test(NAME UT_EmbeddedPythonMasterVolume COMMAND fishpond_p1_embedded_python master-volume)
    add_test(NAME IT_EmbeddedPythonSyntaxLocation COMMAND fishpond_p1_embedded_python syntax-location)
    add_test(NAME IT_PythonExecutionWorkerOffCallerThread COMMAND fishpond_p1_python_worker)
    set_tests_properties(IT_EmbeddedPythonPersistentNamespace IT_EmbeddedPythonErrorIsolation
        IT_EmbeddedPythonSyntaxLocation IT_PythonExecutionWorkerOffCallerThread
        PROPERTIES LABELS "IT;P1;FR-012;FR-013")
    set_tests_properties(UT_EmbeddedPythonNamedPlayerReplacement UT_EmbeddedPythonSilenceAndPanic UT_EmbeddedPythonSinglePlayerStop UT_EmbeddedPythonMultiplePlayerStop
        UT_EmbeddedPythonTargetRequired PROPERTIES LABELS "UT;P1;FR-012;FR-013;SARDINE-COMPAT")
    set_tests_properties(UT_EmbeddedPythonClockState PROPERTIES LABELS "UT;P1;FR-016")
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

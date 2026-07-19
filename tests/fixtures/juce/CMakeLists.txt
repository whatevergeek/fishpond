# This target proves that the locked JUCE source can build a deterministic VST3
# instrument fixture. It is deliberately outside the offline P0 contract path.
set(FISHPOND_FIXTURE_FORMATS VST3)
if(APPLE)
    list(APPEND FISHPOND_FIXTURE_FORMATS AU)
endif()

juce_add_plugin(FishpondFixtureInstrument
    COMPANY_NAME Fishpond
    PLUGIN_MANUFACTURER_CODE Fshp
    PLUGIN_CODE Fpfi
    FORMATS ${FISHPOND_FIXTURE_FORMATS}
    PRODUCT_NAME "Fishpond Fixture Instrument"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(FishpondFixtureInstrument PRIVATE FixtureInstrument.cpp)
target_link_libraries(FishpondFixtureInstrument PRIVATE juce::juce_audio_utils)
# The fixture has no VST2 release history, so it must not advertise VST2
# replacement compatibility to JUCE's VST3 wrapper.
target_compile_definitions(FishpondFixtureInstrument PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)

add_executable(fishpond_juce_lifecycle_spike
    FixtureLifecycleSpike.cpp
    FixtureInstrument.cpp
)
target_link_libraries(fishpond_juce_lifecycle_spike PRIVATE juce::juce_audio_utils)

add_executable(fishpond_single_channel_host_spike
    SingleChannelHostSpike.cpp
    FixtureInstrument.cpp
    "${PROJECT_SOURCE_DIR}/src/host/SingleChannelHost.cpp"
)
target_include_directories(fishpond_single_channel_host_spike PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(fishpond_single_channel_host_spike PRIVATE juce::juce_audio_utils)

if(UNIX AND NOT APPLE)
    find_package(CURL REQUIRED)
    target_link_libraries(FishpondFixtureInstrument PRIVATE CURL::libcurl)
    target_link_libraries(fishpond_juce_lifecycle_spike PRIVATE CURL::libcurl)
endif()

add_custom_target(run-juce-lifecycle-spike
    COMMAND fishpond_juce_lifecycle_spike
    DEPENDS fishpond_juce_lifecycle_spike
    VERBATIM
)

add_custom_target(run-single-channel-host-spike
    COMMAND fishpond_single_channel_host_spike
    DEPENDS fishpond_single_channel_host_spike
    VERBATIM
)

add_executable(fishpond_graph_handoff_host_spike
    GraphHandoffHostSpike.cpp
    FixtureInstrument.cpp
    "${PROJECT_SOURCE_DIR}/src/host/SingleChannelHost.cpp"
)
target_include_directories(fishpond_graph_handoff_host_spike PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(fishpond_graph_handoff_host_spike PRIVATE juce::juce_audio_utils)

add_custom_target(run-graph-handoff-host-spike
    COMMAND fishpond_graph_handoff_host_spike
    DEPENDS fishpond_graph_handoff_host_spike
    VERBATIM
)

add_executable(fishpond_vst3_fixture_load_spike
    VST3FixtureLoadSpike.cpp
    "${PROJECT_SOURCE_DIR}/src/host/ControlledVST3Bass.cpp"
    "${PROJECT_SOURCE_DIR}/src/host/SingleChannelHost.cpp"
)
target_include_directories(fishpond_vst3_fixture_load_spike PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(fishpond_vst3_fixture_load_spike PRIVATE juce::juce_audio_utils)
if(APPLE)
    set(FISHPOND_FIXTURE_VST3_LOAD_PATH "$<TARGET_BUNDLE_DIR:FishpondFixtureInstrument_VST3>")
else()
    set(FISHPOND_FIXTURE_VST3_LOAD_PATH "$<TARGET_FILE:FishpondFixtureInstrument_VST3>")
endif()
target_compile_definitions(fishpond_vst3_fixture_load_spike PRIVATE
    JUCE_PLUGINHOST_VST3=1
    FISHPOND_FIXTURE_VST3_PATH="${FISHPOND_FIXTURE_VST3_LOAD_PATH}"
)
add_dependencies(fishpond_vst3_fixture_load_spike FishpondFixtureInstrument_VST3)

add_custom_target(run-vst3-fixture-load-spike
    COMMAND fishpond_vst3_fixture_load_spike
    DEPENDS fishpond_vst3_fixture_load_spike
    VERBATIM
)

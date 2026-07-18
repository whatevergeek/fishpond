# This target proves that the locked JUCE source can build a deterministic VST3
# instrument fixture. It is deliberately outside the offline P0 contract path.
juce_add_plugin(FishpondFixtureInstrument
    COMPANY_NAME Fishpond
    PLUGIN_MANUFACTURER_CODE Fshp
    PLUGIN_CODE Fpfi
    FORMATS VST3 AU
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

add_custom_target(run-juce-lifecycle-spike
    COMMAND fishpond_juce_lifecycle_spike
    DEPENDS fishpond_juce_lifecycle_spike
    VERBATIM
)

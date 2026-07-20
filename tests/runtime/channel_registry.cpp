#include "mixer/ChannelRegistry.h"
#include "runtime/Runtime.h"

#include <iostream>

int main(int argc, char** argv)
{
    const std::string test = argc == 2 ? argv[1] : "";
    fishpond::ChannelRegistry registry;

    if (test == "aliases") {
        const auto bass = registry.add("Bass");
        const auto glassPad = registry.add("Glass Pad");
        const auto resolved = registry.resolve("GLASS_PAD");
        return bass && glassPad && resolved && bass->alias == "bass" && glassPad->alias == "glass_pad"
            && resolved->id == glassPad->id ? 0 : 1;
    }
    if (test == "collisions") {
        return registry.add("Glass Pad") && ! registry.add("glass_pad") && ! registry.add("GLASS PAD") ? 0 : 1;
    }
    if (test == "rename") {
        const auto bass = registry.add("Bass");
        if (! bass || ! registry.rename(bass->id, "Low End")) return 1;
        const auto resolved = registry.resolve("low_end");
        return resolved && resolved->id == bass->id && resolved->paneName == "Low End" ? 0 : 1;
    }
    if (test == "target-routing") {
        const auto lead = registry.add("Glass Pad");
        fishpond::Runtime runtime;
        return lead && runtime.evaluateEditorText("Pa >> n(\"C4\", target=\"glass_pad\", p=1)", registry, { lead->id }).accepted
            && ! runtime.evaluateEditorText("Pa >> n(\"C4\", target=\"missing\", p=1)", registry, { lead->id }).accepted ? 0 : 1;
    }
    if (test == "four-instrument-routing") {
        const auto one = registry.add("Instrument 01");
        const auto two = registry.add("Instrument 02");
        const auto three = registry.add("Instrument 03");
        const auto four = registry.add("Instrument 04");
        fishpond::Runtime runtime;
        if (! one || ! two || ! three || ! four)
            return 1;
        const std::vector<std::uint64_t> ready { one->id, two->id, three->id, four->id };
        return runtime.evaluateEditorText("Pa >> n(\"C3\", target=\"instrument_01\", p=1)", registry, ready).accepted
            && runtime.evaluateEditorText("Pb >> n(\"D3\", target=\"instrument_02\", p=1)", registry, ready).accepted
            && runtime.evaluateEditorText("Pc >> n(\"E3\", target=\"instrument_03\", p=1)", registry, ready).accepted
            && runtime.evaluateEditorText("Pd >> n(\"F3\", target=\"instrument_04\", p=1)", registry, ready).accepted ? 0 : 1;
    }

    std::cerr << "unknown test: " << test << '\n';
    return 2;
}

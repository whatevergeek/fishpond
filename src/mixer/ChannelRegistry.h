#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fishpond {

struct ChannelIdentity {
    std::uint64_t id {};
    std::string paneName;
    std::string alias;
};

// Management-thread model for user-facing channel names. The stable id is
// intentionally independent of the derived live-code alias, so a rename does
// not change the channel's audio-graph identity.
class ChannelRegistry {
public:
    std::optional<ChannelIdentity> add(std::string paneName);
    bool rename(std::uint64_t id, std::string paneName);
    std::optional<ChannelIdentity> resolve(const std::string& nameOrAlias) const;
    const std::vector<ChannelIdentity>& channels() const noexcept { return entries; }

    static std::string canonicalAlias(const std::string& paneName);

private:
    bool aliasAvailable(const std::string& alias, std::optional<std::uint64_t> ignoredId = std::nullopt) const;

    std::uint64_t nextId { 1 };
    std::vector<ChannelIdentity> entries;
};

} // namespace fishpond

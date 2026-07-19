#include "mixer/ChannelRegistry.h"

#include <cctype>

namespace fishpond {

std::string ChannelRegistry::canonicalAlias(const std::string& paneName)
{
    std::string alias;
    bool previousSeparator = false;
    for (const auto character : paneName) {
        const auto value = static_cast<unsigned char>(character);
        if (std::isalnum(value)) {
            alias += static_cast<char>(std::tolower(value));
            previousSeparator = false;
        } else if (! alias.empty() && ! previousSeparator) {
            alias += '_';
            previousSeparator = true;
        }
    }
    if (! alias.empty() && alias.back() == '_')
        alias.pop_back();
    return alias;
}

std::optional<ChannelIdentity> ChannelRegistry::add(std::string paneName)
{
    const auto alias = canonicalAlias(paneName);
    if (alias.empty() || ! aliasAvailable(alias))
        return std::nullopt;
    entries.push_back({ nextId++, std::move(paneName), alias });
    return entries.back();
}

bool ChannelRegistry::rename(const std::uint64_t id, std::string paneName)
{
    const auto alias = canonicalAlias(paneName);
    if (alias.empty() || ! aliasAvailable(alias, id))
        return false;
    for (auto& entry : entries) {
        if (entry.id == id) {
            entry.paneName = std::move(paneName);
            entry.alias = alias;
            return true;
        }
    }
    return false;
}

std::optional<ChannelIdentity> ChannelRegistry::resolve(const std::string& nameOrAlias) const
{
    const auto alias = canonicalAlias(nameOrAlias);
    for (const auto& entry : entries)
        if (entry.alias == alias)
            return entry;
    return std::nullopt;
}

bool ChannelRegistry::aliasAvailable(const std::string& alias, const std::optional<std::uint64_t> ignoredId) const
{
    for (const auto& entry : entries)
        if (entry.alias == alias && (! ignoredId || entry.id != *ignoredId))
            return false;
    return true;
}

} // namespace fishpond

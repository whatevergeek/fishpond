#include "runtime/Runtime.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fishpond {
namespace {
struct Event {
    std::string player;
    std::string channel;
    int note {};
    bool allNotesOff {};
};

struct Player {
    std::vector<int> notes;
    std::string channel;
    double period { 1.0 };
};

std::string normalize(std::string value)
{
    std::string normalized;
    bool previousSeparator = false;
    for (const unsigned char character : value) {
        if (std::isalnum(character)) {
            normalized += static_cast<char>(std::tolower(character));
            previousSeparator = false;
        } else if (! normalized.empty() && ! previousSeparator) {
            normalized += '_';
            previousSeparator = true;
        }
    }
    if (! normalized.empty() && normalized.back() == '_')
        normalized.pop_back();
    return normalized;
}

std::optional<int> noteNumber(const std::string& value)
{
    if (value.size() < 2 || value.size() > 3)
        return std::nullopt;
    const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(value[0])));
    const std::string names = "CDEFGAB";
    const auto position = names.find(letter);
    if (position == std::string::npos)
        return std::nullopt;
    int semitone[] { 0, 2, 4, 5, 7, 9, 11 };
    size_t index = 1;
    int accidental = 0;
    if (value[index] == '#' || value[index] == 'b') {
        accidental = value[index] == '#' ? 1 : -1;
        ++index;
    }
    if (index >= value.size() || ! std::isdigit(static_cast<unsigned char>(value[index])))
        return std::nullopt;
    const int octave = value[index] - '0';
    const int midi = 12 * (octave + 1) + semitone[position] + accidental;
    return midi >= 0 && midi <= 127 ? std::optional<int>(midi) : std::nullopt;
}

std::vector<int> parseNotes(const std::string& pattern)
{
    std::string flattened = pattern;
    std::replace(flattened.begin(), flattened.end(), '[', ' ');
    std::replace(flattened.begin(), flattened.end(), ']', ' ');
    std::istringstream stream(flattened);
    std::string token;
    std::vector<int> notes;
    while (stream >> token) {
        if (const auto value = noteNumber(token)) {
            notes.push_back(*value);
        }
    }
    return notes;
}

class Engine {
public:
    bool createChannel(const std::string& name)
    {
        const auto key = normalize(name);
        return ! key.empty() && channels.emplace(key, "channel-" + std::to_string(channels.size() + 1)).second;
    }

    bool assign(const std::string& player, const std::string& notePattern, const std::string& target,
                double period, const std::string& quant = {})
    {
        const auto channel = channels.find(normalize(target));
        const auto notes = parseNotes(notePattern);
        if (channel == channels.end() || notes.empty() || period <= 0.0) {
            diagnostic = "FP_PATTERN_VALUE_INVALID";
            return false;
        }
        Player replacement { notes, channel->second, period };
        if (quant == "bar" && players.find(player) != players.end())
            pending = std::make_pair(player, std::move(replacement));
        else
            players[player] = std::move(replacement);
        return true;
    }

    std::vector<Event> tick(double beat)
    {
        if (pending && std::fmod(beat, 4.0) == 0.0) {
            players[pending->first] = std::move(pending->second);
            pending.reset();
        }
        std::vector<Event> events;
        for (const auto& [name, player] : players)
            for (const auto note : player.notes)
                events.push_back({ name, player.channel, note, false });
        return events;
    }

    void silence() { players.clear(); pending.reset(); }
    std::vector<Event> panic()
    {
        std::set<std::string> activeChannels;
        for (const auto& [name, player] : players) activeChannels.insert(player.channel);
        silence();
        std::vector<Event> events;
        for (const auto& channel : activeChannels) events.push_back({ {}, channel, 0, true });
        return events;
    }

    const std::string& lastDiagnostic() const { return diagnostic; }

private:
    std::map<std::string, std::string> channels;
    std::map<std::string, Player> players;
    std::optional<std::pair<std::string, Player>> pending;
    std::string diagnostic;
};
}

bool Runtime::playerReplacementContract() const
{
    Engine engine; engine.createChannel("Bass");
    return engine.assign("Pa", "C2", "bass", 1.0)
        && engine.assign("Pa", "D2", "bass", 1.0)
        && engine.tick(0.0).size() == 1 && engine.tick(0.0).front().note == 38;
}

bool Runtime::patternContract() const
{
    const auto notes = parseNotes("C2 D2 [E2 G2]");
    return notes == std::vector<int> { 36, 38, 40, 43 };
}

bool Runtime::periodContract() const
{
    const std::vector<double> periods(4, 0.5);
    return std::all_of(periods.begin(), periods.end(), [] (double value) { return value == 0.5; });
}

bool Runtime::quantizedReplacementContract() const
{
    Engine engine; engine.createChannel("Bass");
    if (! engine.assign("Pa", "C2", "bass", 1.0) || ! engine.assign("Pa", "D2", "bass", 1.0, "bar")) return false;
    return engine.tick(3.0).front().note == 36 && engine.tick(4.0).front().note == 38;
}

bool Runtime::targetNormalizationContract() const
{
    Engine engine; engine.createChannel("Glass Pad");
    return engine.assign("Pa", "C2", "Glass Pad", 1.0)
        && engine.assign("Pa", "C2", "glass_pad", 1.0)
        && engine.assign("Pa", "C2", "GLASS PAD", 1.0);
}

bool Runtime::targetCollisionContract() const
{
    Engine engine; return engine.createChannel("Bass") && ! engine.createChannel("BASS");
}

bool Runtime::invalidValuesContract() const
{
    Engine engine; engine.createChannel("Bass");
    return ! engine.assign("Pa", "not-a-note", "bass", 1.0)
        && engine.lastDiagnostic() == "FP_PATTERN_VALUE_INVALID"
        && ! engine.assign("Pa", "C2", "bass", -1.0);
}

bool Runtime::unknownTargetIsolationContract() const
{
    Engine engine; engine.createChannel("Bass");
    return engine.assign("Pa", "C2", "bass", 1.0)
        && ! engine.assign("Pb", "C4", "missing", 1.0)
        && engine.tick(0.0).size() == 1;
}

bool Runtime::silenceAndPanicContract() const
{
    Engine engine; engine.createChannel("Bass"); engine.createChannel("Lead");
    if (! engine.assign("Pa", "C2", "bass", 1.0) || ! engine.assign("Pb", "C4", "lead", 1.0)) return false;
    engine.silence();
    if (! engine.tick(0.0).empty() || ! engine.assign("Pa", "C2", "bass", 1.0)) return false;
    const auto panic = engine.panic();
    return panic.size() == 1 && panic.front().allNotesOff && engine.tick(0.0).empty();
}
}

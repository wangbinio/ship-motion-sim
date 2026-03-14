#include "scenario/command_schedule.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ship_sim {

namespace {

std::string trim(std::string text) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
    text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
    return text;
}

CommandType parseCommandType(const std::string& value) {
    if (value == "rudder") {
        return CommandType::Rudder;
    }
    if (value == "engine") {
        return CommandType::Engine;
    }
    throw std::runtime_error("Unknown command type: " + value);
}

CommandEvent parseCsvLine(const std::string& line, std::size_t line_number) {
    std::stringstream stream(line);
    std::string time_text;
    std::string type_text;
    std::string value_text;

    if (!std::getline(stream, time_text, ',') ||
        !std::getline(stream, type_text, ',') ||
        !std::getline(stream, value_text)) {
        throw std::runtime_error("Invalid command CSV format at line " + std::to_string(line_number));
    }

    CommandEvent event;
    try {
        event.time_s = std::stod(trim(time_text));
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid time_s at line " + std::to_string(line_number));
    }
    event.type = parseCommandType(trim(type_text));
    event.value = trim(value_text);
    if (event.value.empty()) {
        throw std::runtime_error("Empty command value at line " + std::to_string(line_number));
    }
    return event;
}

}  // namespace

CommandSchedule::CommandSchedule(CommandEvents events) : events_(std::move(events)) {
    std::stable_sort(
        events_.begin(),
        events_.end(),
        [](const CommandEvent& lhs, const CommandEvent& rhs) { return lhs.time_s < rhs.time_s; });
}

CommandSchedule CommandSchedule::loadFromCsvFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open commands file: " + path);
    }

    CommandEvents events;
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        if (line_number == 1 && trimmed == "time_s,type,value") {
            continue;
        }
        events.push_back(parseCsvLine(trimmed, line_number));
    }

    return CommandSchedule(std::move(events));
}

CommandEvents CommandSchedule::popReadyEvents(double sim_time_s) {
    constexpr double kTimeTolerance = 1e-9;

    CommandEvents ready;
    while (next_index_ < events_.size() && events_[next_index_].time_s <= sim_time_s + kTimeTolerance) {
        ready.push_back(events_[next_index_]);
        ++next_index_;
    }
    return ready;
}

bool CommandSchedule::empty() const {
    return next_index_ >= events_.size();
}

}  // namespace ship_sim

#include "scenario/csv_command_io.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
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

std::string formatCommandType(const CommandType type) {
    switch (type) {
    case CommandType::Rudder:
        return "rudder";
    case CommandType::Engine:
        return "engine";
    }
    throw std::runtime_error("Unsupported command type");
}

CommandEvent parseCsvLine(const std::string& line, const std::size_t line_number) {
    std::stringstream stream(line);
    std::string time_text;
    std::string type_text;
    std::string value_text;

    if (!std::getline(stream, time_text, ',') || !std::getline(stream, type_text, ',') ||
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

CommandEvents CsvCommandReader::loadFromFile(const std::string& path) {
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

    return events;
}

void CsvCommandWriter::writeToFile(const std::string& path, const CommandEvents& events) {
    const std::filesystem::path output_path(path);
    if (output_path.has_parent_path()) {
        std::filesystem::create_directories(output_path.parent_path());
    }

    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Failed to open command output file: " + path);
    }

    output << "time_s,type,value\n";
    for (const auto& event : events) {
        output << event.time_s << ',' << formatCommandType(event.type) << ',' << event.value << '\n';
    }
}

}  // namespace ship_sim

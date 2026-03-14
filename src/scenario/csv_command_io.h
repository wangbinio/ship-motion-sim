#pragma once

#include <string>

#include "common/types.h"

namespace ship_sim {

class CsvCommandReader {
public:
    static CommandEvents loadFromFile(const std::string& path);
};

class CsvCommandWriter {
public:
    static void writeToFile(const std::string& path, const CommandEvents& events);
};

}  // namespace ship_sim

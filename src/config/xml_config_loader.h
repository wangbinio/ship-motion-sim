#pragma once

#include <string>

#include "common/types.h"

namespace ship_sim {

class XmlConfigLoader {
public:
    static SessionConfig loadFromFile(const std::string& path);
};

class XmlConfigWriter {
public:
    static void saveToFile(const SessionConfig& config, const std::string& path);
};

}  // namespace ship_sim

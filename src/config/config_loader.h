#pragma once

#include <string>

#include "common/types.h"

namespace ship_sim {

class ConfigLoader {
public:
    static AppConfig loadFromFile(const std::string& path);
};

}  // namespace ship_sim

#pragma once

#include <string>

namespace ship_sim {

struct CliOptions {
    std::string config_path;
    std::string commands_path;
    std::string output_path;
};

class SimulationApp {
public:
    int run(const CliOptions& options) const;
};

CliOptions parseCliOptions(int argc, char* argv[]);

}  // namespace ship_sim

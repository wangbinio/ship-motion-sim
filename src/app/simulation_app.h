#pragma once

#include <string>

namespace ship_sim {

struct CliOptions {
    std::string config_path;
    std::string commands_path;
    std::string output_path;
    std::string artifacts_dir;
    std::string plot_output_path;
    bool enable_plot {true};
};

class SimulationApp {
public:
    int run(const CliOptions& options) const;
};

CliOptions parseCliOptions(int argc, char* argv[]);

}  // namespace ship_sim

#include "app/simulation_app.h"

#include <iostream>
#include <stdexcept>

#include "app/batch_simulation_runner.h"
#include "config/xml_config_loader.h"

namespace ship_sim {

namespace {

void printUsage() {
    std::cerr << "Usage: ship_motion_sim --config <path> --commands <path> "
                 "[--output <path>] [--artifacts-dir <dir>] [--plot-output <path>] [--no-plot]\n";
}

}  // namespace

CliOptions parseCliOptions(int argc, char* argv[]) {
    CliOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            options.config_path = argv[++i];
        } else if (arg == "--commands" && i + 1 < argc) {
            options.commands_path = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            options.output_path = argv[++i];
        } else if (arg == "--artifacts-dir" && i + 1 < argc) {
            options.artifacts_dir = argv[++i];
        } else if (arg == "--plot-output" && i + 1 < argc) {
            options.plot_output_path = argv[++i];
        } else if (arg == "--no-plot") {
            options.enable_plot = false;
        } else {
            throw std::runtime_error("Unknown or incomplete argument: " + arg);
        }
    }

    if (options.config_path.empty() || options.commands_path.empty()) {
        printUsage();
        throw std::runtime_error("Both --config and --commands are required");
    }

    if (!options.plot_output_path.empty() && options.output_path.empty() && options.artifacts_dir.empty()) {
        throw std::runtime_error("--plot-output requires --output or --artifacts-dir");
    }

    return options;
}

int SimulationApp::run(const CliOptions& options) const {
    BatchSimulationRunner runner;
    const BatchRunResult result = runner.run(BatchRunOptions {
        XmlConfigLoader::loadFromFile(options.config_path),
        options.commands_path,
        options.output_path,
        options.artifacts_dir,
        options.plot_output_path,
        options.enable_plot,
    });

    if (!result.artifacts.summary_path.empty()) {
        std::cerr << "Summary: " << result.artifacts.summary_path << '\n';
    }
    return 0;
}

}  // namespace ship_sim

#include "app/simulation_app.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "config/config_loader.h"
#include "logger/state_logger.h"
#include "model/simple_nomoto_ship_model.h"
#include "scenario/command_schedule.h"

namespace ship_sim {

namespace {

void applyEvent(
    SimpleNomotoShipModel& model,
    const CommandEvent& event,
    const SimulationConfig& simulation_config) {
    if (event.type == CommandType::Engine) {
        model.setEngineCommand(event.value);
        return;
    }

    if (event.type == CommandType::Rudder) {
        try {
            const double rudder_deg = std::stod(event.value);
            if (std::abs(rudder_deg) > simulation_config.rudder_limit_deg) {
                std::cerr << "Warning: rudder command " << rudder_deg
                          << " deg exceeds limit " << simulation_config.rudder_limit_deg
                          << " deg and will be clamped\n";
            }
            model.setRudderCommandDeg(rudder_deg);
        } catch (const std::exception&) {
            throw std::runtime_error("Invalid rudder command value: " + event.value);
        }
        return;
    }

    throw std::runtime_error("Unsupported command type");
}

void printUsage() {
    std::cerr
        << "Usage: ship_motion_sim --config <path> --commands <path> [--output <path>]\n";
}

std::unique_ptr<std::ofstream> openOutputFile(const std::string& output_path) {
    auto file_stream = std::make_unique<std::ofstream>();
    const std::filesystem::path path(output_path);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    file_stream->open(path);
    if (!(*file_stream)) {
        throw std::runtime_error("Failed to open output file: " + output_path);
    }
    return file_stream;
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
        } else {
            throw std::runtime_error("Unknown or incomplete argument: " + arg);
        }
    }

    if (options.config_path.empty() || options.commands_path.empty()) {
        printUsage();
        throw std::runtime_error("Both --config and --commands are required");
    }

    return options;
}

int SimulationApp::run(const CliOptions& options) const {
    const AppConfig config = ConfigLoader::loadFromFile(options.config_path);
    CommandSchedule schedule = CommandSchedule::loadFromCsvFile(options.commands_path);

    SimpleNomotoShipModel model(config.simulation);
    model.setEngineOrderMapping(config.engine_order_map);
    model.setInitialState(config.initial_state);

    std::unique_ptr<std::ofstream> file_stream;
    std::ostream* output_stream = &std::cout;

    if (!options.output_path.empty()) {
        file_stream = openOutputFile(options.output_path);
        output_stream = file_stream.get();
    }

    StateLogger logger(*output_stream);
    logger.writeHeader();
    logger.writeState(model.getState(0.0));

    for (double sim_time = 0.0;
         sim_time + config.simulation.dt_s <= config.simulation.duration_s + 1e-9;
         sim_time += config.simulation.dt_s) {
        for (const auto& event : schedule.popReadyEvents(sim_time)) {
            applyEvent(model, event, config.simulation);
        }
        model.step(config.simulation.dt_s);
        logger.writeState(model.getState(sim_time + config.simulation.dt_s));
    }

    return 0;
}

}  // namespace ship_sim

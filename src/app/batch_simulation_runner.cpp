#include "app/batch_simulation_runner.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "app/plot_runner.h"
#include "app/simulation_session.h"
#include "logger/state_logger.h"
#include "scenario/command_schedule.h"
#include "scenario/csv_command_io.h"

namespace ship_sim {

namespace {

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

ArtifactPaths resolveArtifactPaths(const BatchRunOptions& options) {
    ArtifactPaths paths;
    paths.artifacts_dir = options.artifacts_dir;

    if (!options.artifacts_dir.empty()) {
        std::filesystem::create_directories(options.artifacts_dir);
        paths.state_csv_path =
            options.output_path.empty() ? (std::filesystem::path(options.artifacts_dir) / "state.csv").string()
                                        : options.output_path;
        paths.commands_csv_path = (std::filesystem::path(options.artifacts_dir) / "commands.csv").string();
        paths.summary_path = (std::filesystem::path(options.artifacts_dir) / "summary.txt").string();
        paths.plot_path =
            options.plot_output_path.empty() ? (std::filesystem::path(options.artifacts_dir) / "plot.png").string()
                                             : options.plot_output_path;
        return paths;
    }

    paths.state_csv_path = options.output_path;
    paths.plot_path = options.plot_output_path;
    return paths;
}

void writeStateHistory(const std::string& path, const std::vector<ShipState>& states) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Failed to open state output file: " + path);
    }
    StateLogger logger(output);
    logger.writeHeader();
    for (const auto& state : states) {
        logger.writeState(state);
    }
}

void writeSummary(
    const std::string& path,
    const BatchRunResult& result,
    const SessionConfig& config,
    const std::string& commands_path) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Failed to open summary output file: " + path);
    }

    output << std::fixed << std::setprecision(6);
    output << "config_default_output_dir=" << config.default_output_dir << '\n';
    output << "input_commands_path=" << commands_path << '\n';
    output << "state_csv=" << result.artifacts.state_csv_path << '\n';
    output << "commands_csv=" << result.artifacts.commands_csv_path << '\n';
    output << "plot_png=" << result.artifacts.plot_path << '\n';
    output << "state_count=" << result.state_count << '\n';
    output << "command_count=" << result.command_count << '\n';
    output << "final_time_s=" << result.final_state.time_s << '\n';
    output << "final_lat_deg=" << result.final_state.lat_deg << '\n';
    output << "final_lon_deg=" << result.final_state.lon_deg << '\n';
    output << "final_heading_deg=" << result.final_state.heading_deg << '\n';
    output << "final_speed_mps=" << result.final_state.speed_mps << '\n';
    output << "final_yaw_rate_deg_s=" << result.final_state.yaw_rate_deg_s << '\n';
    output << "plot_generated=" << (result.plot_generated ? "true" : "false") << '\n';
}

}  // namespace

BatchRunResult BatchSimulationRunner::run(const BatchRunOptions& options) const {
    const CommandEvents input_events = CsvCommandReader::loadFromFile(options.commands_path);
    CommandSchedule schedule(input_events);
    SimulationSession session(options.session_config);

    BatchRunResult result;
    result.artifacts = resolveArtifactPaths(options);

    std::unique_ptr<std::ofstream> file_stream;
    std::ostream* output_stream = &std::cout;

    if (!result.artifacts.state_csv_path.empty()) {
        file_stream = openOutputFile(result.artifacts.state_csv_path);
        output_stream = file_stream.get();
    }

    StateLogger logger(*output_stream);
    logger.writeHeader();
    logger.writeState(session.currentState());

    for (double sim_time = 0.0;
         sim_time + options.session_config.simulation.dt_s <= options.session_config.simulation.duration_s + 1e-9;
         sim_time += options.session_config.simulation.dt_s) {
        for (const auto& event : schedule.popReadyEvents(sim_time)) {
            session.applyCommand(event);
        }
        logger.writeState(session.step(options.session_config.simulation.dt_s));
    }

    result.final_state = session.currentState();
    result.state_count = session.stateHistory().size();
    result.command_count = session.commandHistory().size();

    if (!result.artifacts.commands_csv_path.empty()) {
        CsvCommandWriter::writeToFile(result.artifacts.commands_csv_path, session.commandHistory());
    }

    if (options.enable_plot && !result.artifacts.plot_path.empty()) {
        if (result.artifacts.state_csv_path.empty()) {
            throw std::runtime_error("Plot generation requires a state CSV output path");
        }

        std::string commands_csv_path = result.artifacts.commands_csv_path;
        if (commands_csv_path.empty()) {
            commands_csv_path = options.commands_path;
        }

        PlotRunner::run(PlotRequest {
            options.session_config.plot_script_path,
            result.artifacts.state_csv_path,
            commands_csv_path,
            result.artifacts.plot_path,
            "Ship Motion Simulation Overview",
        });
        result.plot_generated = true;
    }

    if (!result.artifacts.summary_path.empty()) {
        writeSummary(result.artifacts.summary_path, result, options.session_config, options.commands_path);
    }

    return result;
}

}  // namespace ship_sim

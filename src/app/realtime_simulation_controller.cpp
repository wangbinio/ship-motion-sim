#include "app/realtime_simulation_controller.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>

#include <QDateTime>

#include "app/plot_runner.h"
#include "logger/state_logger.h"
#include "scenario/csv_command_io.h"

namespace ship_sim {

namespace {

const std::vector<ShipState>& emptyStates() {
    static const std::vector<ShipState> states;
    return states;
}

const CommandEvents& emptyCommands() {
    static const CommandEvents commands;
    return commands;
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
    const SimulationSession& session,
    const ArtifactPaths& artifacts,
    const bool plot_generated) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Failed to open summary output file: " + path);
    }

    const ShipState& final_state = session.currentState();
    output << std::fixed << std::setprecision(6);
    output << "artifacts_dir=" << artifacts.artifacts_dir << '\n';
    output << "state_csv=" << artifacts.state_csv_path << '\n';
    output << "commands_csv=" << artifacts.commands_csv_path << '\n';
    output << "plot_png=" << artifacts.plot_path << '\n';
    output << "state_count=" << session.stateHistory().size() << '\n';
    output << "command_count=" << session.commandHistory().size() << '\n';
    output << "final_time_s=" << final_state.time_s << '\n';
    output << "final_lat_deg=" << final_state.lat_deg << '\n';
    output << "final_lon_deg=" << final_state.lon_deg << '\n';
    output << "final_heading_deg=" << final_state.heading_deg << '\n';
    output << "final_speed_mps=" << final_state.speed_mps << '\n';
    output << "final_yaw_rate_deg_s=" << final_state.yaw_rate_deg_s << '\n';
    output << "plot_generated=" << (plot_generated ? "true" : "false") << '\n';
}

}  // namespace

RealtimeSimulationController::RealtimeSimulationController(QObject* parent) : QObject(parent) {
    timer_.setInterval(33);
    connect(&timer_, &QTimer::timeout, this, &RealtimeSimulationController::onTick);
}

void RealtimeSimulationController::start(const SessionConfig& config) {
    config_ = config;
    session_ = std::make_unique<SimulationSession>(config_);
    accumulator_s_ = 0.0;
    last_artifacts_ = ArtifactPaths {};
    running_ = true;
    elapsed_timer_.start();
    timer_.start();
    emit runningChanged(true);
    emit stateAdvanced();
}

void RealtimeSimulationController::stop() {
    if (!session_) {
        return;
    }

    timer_.stop();
    running_ = false;

    bool plot_generated = false;
    try {
        writeSessionArtifacts(&plot_generated);
    } catch (const std::exception& ex) {
        emit errorOccurred(QString::fromStdString(ex.what()));
    }

    emit runningChanged(false);
    emit sessionFinished(QString::fromStdString(last_artifacts_.artifacts_dir));
    emit stateAdvanced();
}

void RealtimeSimulationController::applyRudderCommand(const double rudder_deg) {
    if (!session_) {
        return;
    }

    session_->applyCommand(CommandEvent {
        session_->currentTimeS(),
        CommandType::Rudder,
        std::to_string(rudder_deg),
    });
    emit stateAdvanced();
}

void RealtimeSimulationController::applyEngineCommand(const std::string& order_id) {
    if (!session_) {
        return;
    }

    session_->applyCommand(CommandEvent {
        session_->currentTimeS(),
        CommandType::Engine,
        order_id,
    });
    emit stateAdvanced();
}

void RealtimeSimulationController::advanceByElapsed(const double elapsed_s) {
    if (!running_ || !session_) {
        return;
    }

    accumulator_s_ += elapsed_s;
    const double dt_s = config_.simulation.dt_s;
    int max_catch_up_steps = 5;
    bool advanced = false;

    while (accumulator_s_ + 1e-9 >= dt_s && max_catch_up_steps-- > 0) {
        session_->step(dt_s);
        accumulator_s_ -= dt_s;
        advanced = true;

        if (config_.simulation.duration_s > 0.0 &&
            session_->currentTimeS() + 1e-9 >= config_.simulation.duration_s) {
            break;
        }
    }

    if (advanced) {
        emit stateAdvanced();
    }

    if (config_.simulation.duration_s > 0.0 &&
        session_->currentTimeS() + 1e-9 >= config_.simulation.duration_s) {
        stop();
    }
}

bool RealtimeSimulationController::isRunning() const {
    return running_;
}

bool RealtimeSimulationController::hasSession() const {
    return static_cast<bool>(session_);
}

const ShipState& RealtimeSimulationController::currentState() const {
    if (!session_) {
        throw std::runtime_error("No active session");
    }
    return session_->currentState();
}

const std::vector<ShipState>& RealtimeSimulationController::stateHistory() const {
    if (!session_) {
        return emptyStates();
    }
    return session_->stateHistory();
}

const CommandEvents& RealtimeSimulationController::commandHistory() const {
    if (!session_) {
        return emptyCommands();
    }
    return session_->commandHistory();
}

double RealtimeSimulationController::currentRudderCommandDeg() const {
    if (!session_) {
        return 0.0;
    }
    return session_->currentRudderCommandDeg();
}

const std::string& RealtimeSimulationController::currentEngineOrderId() const {
    static const std::string empty;
    if (!session_) {
        return empty;
    }
    return session_->currentEngineOrderId();
}

const ArtifactPaths& RealtimeSimulationController::lastArtifacts() const {
    return last_artifacts_;
}

void RealtimeSimulationController::onTick() {
    const qint64 elapsed_ms = elapsed_timer_.restart();
    if (elapsed_ms <= 0) {
        return;
    }
    advanceByElapsed(static_cast<double>(elapsed_ms) / 1000.0);
}

ArtifactPaths RealtimeSimulationController::buildArtifactPaths() const {
    ArtifactPaths artifacts;
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const std::filesystem::path artifacts_dir =
        std::filesystem::path(config_.default_output_dir) / timestamp.toStdString();
    std::filesystem::create_directories(artifacts_dir);

    artifacts.artifacts_dir = artifacts_dir.string();
    artifacts.state_csv_path = (artifacts_dir / "state.csv").string();
    artifacts.commands_csv_path = (artifacts_dir / "commands.csv").string();
    artifacts.plot_path = (artifacts_dir / "plot.png").string();
    artifacts.summary_path = (artifacts_dir / "summary.txt").string();
    return artifacts;
}

void RealtimeSimulationController::writeSessionArtifacts(bool* plot_generated) {
    if (!session_) {
        return;
    }

    last_artifacts_ = buildArtifactPaths();
    writeStateHistory(last_artifacts_.state_csv_path, session_->stateHistory());
    CsvCommandWriter::writeToFile(last_artifacts_.commands_csv_path, session_->commandHistory());

    if (plot_generated != nullptr) {
        *plot_generated = false;
    }
    if (config_.auto_plot && !config_.plot_script_path.empty()) {
        try {
            PlotRunner::run(PlotRequest {
                config_.plot_script_path,
                last_artifacts_.state_csv_path,
                last_artifacts_.commands_csv_path,
                last_artifacts_.plot_path,
                "Ship Motion Simulation Overview",
            });
            if (plot_generated != nullptr) {
                *plot_generated = true;
            }
        } catch (const std::exception& ex) {
            emit errorOccurred(QString::fromStdString(ex.what()));
        }
    }

    writeSummary(last_artifacts_.summary_path, *session_, last_artifacts_, plot_generated != nullptr && *plot_generated);
}

}  // namespace ship_sim

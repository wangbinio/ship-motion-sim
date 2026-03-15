#include "app/realtime_simulation_controller.h"

#include <filesystem>
#include <stdexcept>

#include <QDateTime>

#include "logger/state_logger.h"

namespace ship_sim {

RealtimeSimulationController::RealtimeSimulationController(QObject* parent) : QObject(parent) {
    timer_.setInterval(33);
    connect(&timer_, &QTimer::timeout, this, &RealtimeSimulationController::onTick);
}

void RealtimeSimulationController::setSessionConfig(const SessionConfig& config) {
    if (running_) {
        stop();
    }
    config_ = config;
}

void RealtimeSimulationController::stop() {
    if (!session_ && !log_output_.is_open()) {
        return;
    }

    timer_.stop();
    running_ = false;
    session_.reset();
    closeLogFile();
    accumulator_s_ = 0.0;

    emit runningChanged(false);
    emit stateAdvanced();
}

void RealtimeSimulationController::applyRudderCommand(const double rudder_deg) {
    try {
        ensureSessionStarted();
        session_->applyCommand(CommandEvent {
            session_->currentTimeS(),
            CommandType::Rudder,
            std::to_string(rudder_deg),
        });
        emit stateAdvanced();
    } catch (const std::exception& ex) {
        emit errorOccurred(QString::fromStdString(ex.what()));
    }
}

void RealtimeSimulationController::applyEngineCommand(const std::string& order_id) {
    try {
        ensureSessionStarted();
        session_->applyCommand(CommandEvent {
            session_->currentTimeS(),
            CommandType::Engine,
            order_id,
        });
        emit stateAdvanced();
    } catch (const std::exception& ex) {
        emit errorOccurred(QString::fromStdString(ex.what()));
    }
}

void RealtimeSimulationController::advanceByElapsed(const double elapsed_s) {
    if (!running_ || !session_) {
        return;
    }

    // 先累积真实经过的墙钟时间，再按固定 dt 推进模型。
    // 这样即使 Qt 定时器回调存在抖动，数值积分步长也仍然稳定。
    accumulator_s_ += elapsed_s;
    const double dt_s = config_.simulation.dt_s;
    int max_catch_up_steps = 5;
    bool advanced = false;
    bool reached_duration = false;

    // 单次回调最多补若干步，避免界面卡顿后一次性追赶过多积分步数，
    // 反过来进一步拖慢 UI。
    while (accumulator_s_ + 1e-9 >= dt_s && max_catch_up_steps-- > 0) {
        const ShipState state = session_->step(dt_s);
        logger_->writeState(state);
        accumulator_s_ -= dt_s;
        advanced = true;

        if (config_.simulation.duration_s > 0.0 &&
            session_->currentTimeS() + 1e-9 >= config_.simulation.duration_s) {
            reached_duration = true;
            break;
        }
    }

    if (advanced) {
        emit stateAdvanced();
    }

    if (reached_duration) {
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

const std::string& RealtimeSimulationController::logFilePath() const {
    return log_file_path_;
}

void RealtimeSimulationController::onTick() {
    const qint64 elapsed_ms = elapsed_timer_.restart();
    if (elapsed_ms <= 0) {
        return;
    }
    advanceByElapsed(static_cast<double>(elapsed_ms) / 1000.0);
}

void RealtimeSimulationController::ensureSessionStarted() {
    if (session_) {
        return;
    }
    if (config_.simulation.dt_s <= 0.0) {
        throw std::runtime_error("Simulation dt_s must be positive");
    }

    const std::filesystem::path log_path(buildLogFilePath());
    std::filesystem::create_directories(log_path.parent_path());

    log_output_.open(log_path, std::ios::out | std::ios::trunc);
    if (!log_output_) {
        throw std::runtime_error("Failed to open log file: " + log_path.string());
    }

    log_file_path_ = log_path.string();
    logger_ = std::make_unique<StateLogger>(log_output_);
    session_ = std::make_unique<SimulationSession>(config_);
    logger_->writeHeader();
    logger_->writeState(session_->currentState());

    accumulator_s_ = 0.0;
    running_ = true;
    elapsed_timer_.start();
    timer_.start();

    emit runningChanged(true);
    emit stateAdvanced();
}

std::string RealtimeSimulationController::buildLogFilePath() const {
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const std::filesystem::path path =
        std::filesystem::path("..") / "Logs" / (timestamp.toStdString() + ".csv");
    return path.string();
}

void RealtimeSimulationController::closeLogFile() {
    logger_.reset();
    if (log_output_.is_open()) {
        log_output_.flush();
        log_output_.close();
    }
}

}  // namespace ship_sim

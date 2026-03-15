#pragma once

#include <fstream>
#include <memory>
#include <string>

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

#include "app/simulation_session.h"
#include "common/types.h"
#include "logger/state_logger.h"

namespace ship_sim {

class RealtimeSimulationController : public QObject {
    Q_OBJECT

public:
    explicit RealtimeSimulationController(QObject* parent = nullptr);

    void setSessionConfig(const SessionConfig& config);
    void stop();
    void applyRudderCommand(double rudder_deg);
    void applyEngineCommand(const std::string& order_id);
    void advanceByElapsed(double elapsed_s);

    bool isRunning() const;
    bool hasSession() const;
    const ShipState& currentState() const;
    double currentRudderCommandDeg() const;
    const std::string& currentEngineOrderId() const;
    const std::string& logFilePath() const;

signals:
    void stateAdvanced();
    void runningChanged(bool running);
    void errorOccurred(const QString& message);

private slots:
    void onTick();

private:
    void ensureSessionStarted();
    std::string buildLogFilePath() const;
    void closeLogFile();

    QTimer timer_;
    QElapsedTimer elapsed_timer_;
    SessionConfig config_;
    std::unique_ptr<SimulationSession> session_;
    std::unique_ptr<StateLogger> logger_;
    std::ofstream log_output_;
    std::string log_file_path_;
    double accumulator_s_ {0.0};
    bool running_ {false};
};

}  // namespace ship_sim

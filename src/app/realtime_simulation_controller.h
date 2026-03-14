#pragma once

#include <memory>
#include <vector>

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

#include "app/simulation_session.h"
#include "common/types.h"

namespace ship_sim {

class RealtimeSimulationController : public QObject {
    Q_OBJECT

public:
    explicit RealtimeSimulationController(QObject* parent = nullptr);

    void start(const SessionConfig& config);
    void stop();
    void applyRudderCommand(double rudder_deg);
    void applyEngineCommand(const std::string& order_id);
    void advanceByElapsed(double elapsed_s);

    bool isRunning() const;
    bool hasSession() const;
    const ShipState& currentState() const;
    const std::vector<ShipState>& stateHistory() const;
    const CommandEvents& commandHistory() const;
    double currentRudderCommandDeg() const;
    const std::string& currentEngineOrderId() const;
    const ArtifactPaths& lastArtifacts() const;

signals:
    void stateAdvanced();
    void runningChanged(bool running);
    void sessionFinished(const QString& artifacts_dir);
    void errorOccurred(const QString& message);

private slots:
    void onTick();

private:
    ArtifactPaths buildArtifactPaths() const;
    void writeSessionArtifacts(bool* plot_generated);

    QTimer timer_;
    QElapsedTimer elapsed_timer_;
    SessionConfig config_;
    std::unique_ptr<SimulationSession> session_;
    ArtifactPaths last_artifacts_;
    double accumulator_s_ {0.0};
    bool running_ {false};
};

}  // namespace ship_sim

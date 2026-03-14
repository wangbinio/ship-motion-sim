#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "common/types.h"
#include "model/simple_nomoto_ship_model.h"

namespace ship_sim {

class SimulationSession {
public:
    explicit SimulationSession(const SessionConfig& config);

    void applyCommand(const CommandEvent& event);
    ShipState step(double dt_s);

    const SessionConfig& config() const;
    const ShipState& currentState() const;
    double currentTimeS() const;
    double currentRudderCommandDeg() const;
    const std::string& currentEngineOrderId() const;
    const std::vector<ShipState>& stateHistory() const;
    const CommandEvents& commandHistory() const;

private:
    std::unordered_map<std::string, double> buildEngineOrderMap() const;

    SessionConfig config_;
    SimpleNomotoShipModel model_;
    ShipState current_state_ {};
    double current_time_s_ {0.0};
    double current_rudder_deg_ {0.0};
    std::string current_engine_order_id_;
    std::vector<ShipState> state_history_;
    CommandEvents command_history_;
};

}  // namespace ship_sim

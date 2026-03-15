#include "app/simulation_session.h"

#include <cmath>
#include <stdexcept>

namespace ship_sim {

namespace {

double parseRudderValueDeg(const std::string& value) {
    try {
        return std::stod(value);
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid rudder command value: " + value);
    }
}

}  // namespace

SimulationSession::SimulationSession(const SessionConfig& config)
    : config_(config), model_(config.simulation) {
    model_.setEngineOrderMapping(buildEngineOrderMap());
    model_.setInitialState(config_.initial_state);
    current_state_ = model_.getState(0.0);
}

void SimulationSession::applyCommand(const CommandEvent& event) {
    if (event.type == CommandType::Engine) {
        model_.setEngineCommand(event.value);
        current_engine_order_id_ = event.value;
    } else if (event.type == CommandType::Rudder) {
        current_rudder_deg_ = parseRudderValueDeg(event.value);
        model_.setRudderCommandDeg(current_rudder_deg_);
    } else {
        throw std::runtime_error("Unsupported command type");
    }
}

ShipState SimulationSession::step(const double dt_s) {
    model_.step(dt_s);
    current_time_s_ += dt_s;
    current_state_ = model_.getState(current_time_s_);
    return current_state_;
}

const SessionConfig& SimulationSession::config() const {
    return config_;
}

const ShipState& SimulationSession::currentState() const {
    return current_state_;
}

double SimulationSession::currentTimeS() const {
    return current_time_s_;
}

double SimulationSession::currentRudderCommandDeg() const {
    return current_rudder_deg_;
}

const std::string& SimulationSession::currentEngineOrderId() const {
    return current_engine_order_id_;
}

std::unordered_map<std::string, double> SimulationSession::buildEngineOrderMap() const {
    std::unordered_map<std::string, double> engine_order_map;
    for (const auto& order : config_.engine_orders) {
        engine_order_map.emplace(order.id, order.target_speed_mps);
    }
    if (engine_order_map.empty()) {
        throw std::runtime_error("Engine order mapping must not be empty");
    }
    return engine_order_map;
}

}  // namespace ship_sim

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace ship_sim {

struct SimulationConfig {
    double dt_s {};
    double duration_s {};
    double earth_radius_m {};
    double nomoto_T_s {};
    double nomoto_K {};
    double speed_tau_s {};
    double rudder_limit_deg {};
};

struct InitialState {
    double lat_deg {};
    double lon_deg {};
    double heading_deg {};
    double speed_mps {};
};

struct ShipState {
    double time_s {};
    double lat_deg {};
    double lon_deg {};
    double heading_deg {};
    double speed_mps {};
    double yaw_rate_deg_s {};
};

enum class CommandType {
    Rudder,
    Engine
};

struct CommandEvent {
    double time_s {};
    CommandType type {};
    std::string value;
};

struct AppConfig {
    SimulationConfig simulation;
    InitialState initial_state;
    std::unordered_map<std::string, double> engine_order_map;
};

using CommandEvents = std::vector<CommandEvent>;

}  // namespace ship_sim

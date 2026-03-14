#include "config/config_loader.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "common/math_utils.h"
#include "common/simple_json.h"

namespace ship_sim {

namespace {

std::string readTextFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void validateSimulationConfig(const SimulationConfig& config) {
    if (config.dt_s <= 0.0) {
        throw std::runtime_error("simulation.dt_s must be positive");
    }
    if (config.duration_s < 0.0) {
        throw std::runtime_error("simulation.duration_s must be non-negative");
    }
    if (config.earth_radius_m <= 0.0) {
        throw std::runtime_error("simulation.earth_radius_m must be positive");
    }
    if (config.nomoto_T_s <= 0.0) {
        throw std::runtime_error("simulation.nomoto_T_s must be positive");
    }
    if (config.speed_tau_s <= 0.0) {
        throw std::runtime_error("simulation.speed_tau_s must be positive");
    }
    if (config.rudder_limit_deg <= 0.0) {
        throw std::runtime_error("simulation.rudder_limit_deg must be positive");
    }
}

}  // namespace

AppConfig ConfigLoader::loadFromFile(const std::string& path) {
    const json::Value root = json::parse(readTextFile(path));
    const auto& root_object = root.asObject();

    AppConfig config;

    const auto& simulation = root_object.at("simulation").asObject();
    config.simulation.dt_s = simulation.at("dt_s").asNumber();
    config.simulation.duration_s = simulation.at("duration_s").asNumber();
    config.simulation.earth_radius_m = simulation.at("earth_radius_m").asNumber();
    config.simulation.nomoto_T_s = simulation.at("nomoto_T_s").asNumber();
    config.simulation.nomoto_K = simulation.at("nomoto_K").asNumber();
    config.simulation.speed_tau_s = simulation.at("speed_tau_s").asNumber();
    config.simulation.rudder_limit_deg = simulation.at("rudder_limit_deg").asNumber();

    const auto& initial_state = root_object.at("initial_state").asObject();
    config.initial_state.lat_deg = initial_state.at("lat_deg").asNumber();
    config.initial_state.lon_deg = initial_state.at("lon_deg").asNumber();
    config.initial_state.heading_deg = initial_state.at("heading_deg").asNumber();
    config.initial_state.speed_mps = initial_state.at("speed_mps").asNumber();

    const auto& engine_order_map = root_object.at("engine_order_map").asObject();
    if (engine_order_map.empty()) {
        throw std::runtime_error("engine_order_map must not be empty");
    }
    for (const auto& [key, value] : engine_order_map) {
        config.engine_order_map.emplace(key, value.asNumber());
    }

    const double lat0_rad = math::degToRad(config.initial_state.lat_deg);
    if (std::abs(std::cos(lat0_rad)) < 1e-6) {
        throw std::runtime_error("initial_state.lat_deg is too close to poles for local tangent-plane conversion");
    }

    validateSimulationConfig(config.simulation);
    return config;
}

}  // namespace ship_sim

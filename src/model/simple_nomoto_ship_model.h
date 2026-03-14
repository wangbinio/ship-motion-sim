#pragma once

#include <string>
#include <unordered_map>
#include <utility>

#include "common/types.h"

namespace ship_sim {

class SimpleNomotoShipModel {
public:
    explicit SimpleNomotoShipModel(const SimulationConfig& config);

    void setInitialState(const InitialState& state);
    void setRudderCommandDeg(double rudder_deg);
    void setEngineCommand(const std::string& order);
    void setEngineOrderMapping(const std::unordered_map<std::string, double>& mapping);

    void step(double dt_s);
    ShipState getState(double sim_time_s) const;

private:
    void validateConfig() const;
    void updateSpeed(double dt_s);
    void updateYawRate(double dt_s);
    void updateHeading(double dt_s);
    void updatePosition(double dt_s);
    double clampRudderRad(double rudder_rad) const;
    std::pair<double, double> toLatLonDeg() const;

    SimulationConfig config_;
    InitialState initial_state_;
    std::unordered_map<std::string, double> engine_order_mapping_;

    double lat0_deg_ {0.0};
    double lon0_deg_ {0.0};
    double x_m_ {0.0};
    double y_m_ {0.0};
    double heading_rad_ {0.0};
    double speed_mps_ {0.0};
    double yaw_rate_rps_ {0.0};
    double rudder_cmd_rad_ {0.0};
    double u_cmd_mps_ {0.0};
};

}  // namespace ship_sim

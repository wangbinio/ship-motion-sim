#include "model/simple_nomoto_ship_model.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "common/math_utils.h"

namespace ship_sim
{

SimpleNomotoShipModel::SimpleNomotoShipModel(const SimulationConfig& config) : config_(config)
{
    validateConfig();
}

void SimpleNomotoShipModel::setInitialState(const InitialState& state)
{
    initial_state_ = state;
    lat0_deg_ = state.lat_deg;
    lon0_deg_ = state.lon_deg;
    x_m_ = 0.0;
    y_m_ = 0.0;
    heading_rad_ = math::normalizeAngle0To2Pi(math::degToRad(state.heading_deg));
    speed_mps_ = state.speed_mps;
    yaw_rate_rps_ = 0.0;
    rudder_cmd_rad_ = 0.0;
    u_cmd_mps_ = state.speed_mps;
}

void SimpleNomotoShipModel::setRudderCommandDeg(double rudder_deg)
{
    rudder_cmd_rad_ = clampRudderRad(math::degToRad(rudder_deg));
}

void SimpleNomotoShipModel::setEngineCommand(const std::string& order)
{
    const auto it = engine_order_mapping_.find(order);
    if (it == engine_order_mapping_.end())
    {
        throw std::runtime_error("Unknown engine order: " + order);
    }
    u_cmd_mps_ = it->second;
}

void SimpleNomotoShipModel::setEngineOrderMapping(const std::unordered_map<std::string, double>& mapping)
{
    if (mapping.empty())
    {
        throw std::runtime_error("Engine order mapping must not be empty");
    }
    engine_order_mapping_ = mapping;
}

void SimpleNomotoShipModel::step(double dt_s)
{
    if (dt_s <= 0.0)
    {
        throw std::runtime_error("step dt_s must be positive");
    }

    updateSpeed(dt_s);
    updateYawRate(dt_s);
    updateHeading(dt_s);
    updatePosition(dt_s);
}

ShipState SimpleNomotoShipModel::getState(double sim_time_s) const
{
    const auto [lat_deg, lon_deg] = toLatLonDeg();
    return ShipState{
      sim_time_s, lat_deg, lon_deg, math::radToDeg(heading_rad_), speed_mps_, math::radToDeg(yaw_rate_rps_)};
}

void SimpleNomotoShipModel::validateConfig() const
{
    if (config_.dt_s <= 0.0)
    {
        throw std::runtime_error("Simulation dt_s must be positive");
    }
    if (config_.earth_radius_m <= 0.0)
    {
        throw std::runtime_error("Earth radius must be positive");
    }
    if (config_.nomoto_T_s <= 0.0)
    {
        throw std::runtime_error("Nomoto T must be positive");
    }
    if (config_.rudder_full_effect_speed_mps <= 0.0)
    {
        throw std::runtime_error("Rudder full effect speed must be positive");
    }
    if (config_.speed_tau_s <= 0.0)
    {
        throw std::runtime_error("Speed tau must be positive");
    }
    if (config_.rudder_limit_deg <= 0.0)
    {
        throw std::runtime_error("Rudder limit must be positive");
    }
}

void SimpleNomotoShipModel::updateSpeed(double dt_s)
{
    const double u_dot = (u_cmd_mps_ - speed_mps_) / config_.speed_tau_s;
    speed_mps_ += u_dot * dt_s;
}

void SimpleNomotoShipModel::updateYawRate(double dt_s)
{
    // 简化假设：舵效随航速线性增长，零速时无来流则不再产生新的转首力矩。
    const double commanded_yaw_rate = config_.nomoto_K * rudder_cmd_rad_ * computeRudderEffectiveness();
    const double r_dot = (commanded_yaw_rate - yaw_rate_rps_) / config_.nomoto_T_s;
    yaw_rate_rps_ += r_dot * dt_s;
}

double SimpleNomotoShipModel::computeRudderEffectiveness() const
{
    return std::clamp(std::abs(speed_mps_) / config_.rudder_full_effect_speed_mps, 0.0, 1.0);
}

void SimpleNomotoShipModel::updateHeading(double dt_s)
{
    heading_rad_ = math::normalizeAngle0To2Pi(heading_rad_ + yaw_rate_rps_ * dt_s);
}

void SimpleNomotoShipModel::updatePosition(double dt_s)
{
    // 内部航向采用航海约定：0 度指北、顺时针为正。
    // 局部位置仍使用东-北坐标，因此东向分量取 sin，北向分量取 cos。
    x_m_ += speed_mps_ * std::sin(heading_rad_) * dt_s;
    y_m_ += speed_mps_ * std::cos(heading_rad_) * dt_s;
}

double SimpleNomotoShipModel::clampRudderRad(double rudder_rad) const
{
    const double max_rudder_rad = math::degToRad(config_.rudder_limit_deg);
    return std::clamp(rudder_rad, -max_rudder_rad, max_rudder_rad);
}

std::pair<double, double> SimpleNomotoShipModel::toLatLonDeg() const
{
    return math::localOffsetToLatLonDeg(lat0_deg_, lon0_deg_, x_m_, y_m_, config_.earth_radius_m);
}

} // namespace ship_sim

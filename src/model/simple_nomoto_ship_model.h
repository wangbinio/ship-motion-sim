#pragma once

#include <string>
#include <unordered_map>
#include <utility>

#include "common/types.h"

namespace ship_sim {

class SimpleNomotoShipModel {
public:
    // 使用仿真配置初始化一阶 Nomoto 舰船模型。
    explicit SimpleNomotoShipModel(const SimulationConfig& config);

    // 设置初始经纬度、航向和航速，并重置内部状态。
    void setInitialState(const InitialState& state);
    // 设置当前舵令，单位为度，正右负左。
    void setRudderCommandDeg(double rudder_deg);
    // 设置当前车令，按配置映射到目标航速。
    void setEngineCommand(const std::string& order);
    // 设置车令到目标速度的映射表。
    void setEngineOrderMapping(const std::unordered_map<std::string, double>& mapping);

    // 按给定时间步推进一次模型状态。
    void step(double dt_s);
    // 读取当前时刻的外部状态输出。
    ShipState getState(double sim_time_s) const;

private:
    // 校验模型参数是否满足基本物理约束。
    void validateConfig() const;
    // 更新纵向速度的一阶响应。
    void updateSpeed(double dt_s);
    // 更新 Nomoto 转首角速度响应。
    void updateYawRate(double dt_s);
    // 更新航向角，内部采用航海语义。
    void updateHeading(double dt_s);
    // 按局部东-北坐标更新平面位置。
    void updatePosition(double dt_s);
    // 将舵角限制在最大舵角范围内。
    double clampRudderRad(double rudder_rad) const;
    // 将局部东-北位移换算回经纬度。
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

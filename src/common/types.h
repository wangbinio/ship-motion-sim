#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace ship_sim
{

struct SimulationConfig
{
    double dt_s{};
    double duration_s{};
    double earth_radius_m{};
    double nomoto_T_s{};
    double nomoto_K{};
    double rudder_full_effect_speed_mps{};
    double speed_tau_s{};
    double rudder_limit_deg{};
};

struct InitialState
{
    double lat_deg{};
    double lon_deg{};
    double heading_deg{};
    double speed_mps{};
};

struct ShipState
{
    double time_s{};
    double lat_deg{};
    double lon_deg{};
    double heading_deg{};
    double speed_mps{};
    double yaw_rate_deg_s{};
};

enum class CommandType
{
    Rudder,
    Engine
};

struct CommandEvent
{
    double time_s{};
    CommandType type{};
    std::string value;
};

struct EnginePanelDefinition
{
    std::string id;
    std::string label;
    int display_order{};
};

struct EngineOrderDefinition
{
    std::string id;
    std::string label;
    std::string panel_id;
    double target_speed_mps{};
    int display_order{};
};

struct RudderPresetDefinition
{
    std::string id;
    std::string label;
    double angle_deg{};
    int display_order{};
};

struct SessionConfig
{
    SimulationConfig simulation;
    InitialState initial_state;
    std::vector<EnginePanelDefinition> engine_panels;
    std::vector<EngineOrderDefinition> engine_orders;
    std::vector<RudderPresetDefinition> rudder_presets;
    std::string default_output_dir;
    bool auto_plot{true};
    std::string plot_script_path;
};

struct ArtifactPaths
{
    std::string artifacts_dir;
    std::string state_csv_path;
    std::string commands_csv_path;
    std::string plot_path;
    std::string summary_path;
};

struct BatchRunOptions
{
    SessionConfig session_config;
    std::string commands_path;
    std::string output_path;
    std::string artifacts_dir;
    std::string plot_output_path;
    bool enable_plot{true};
};

struct BatchRunResult
{
    ArtifactPaths artifacts;
    ShipState final_state;
    std::size_t state_count{};
    std::size_t command_count{};
    bool plot_generated{false};
};

using CommandEvents = std::vector<CommandEvent>;

} // namespace ship_sim

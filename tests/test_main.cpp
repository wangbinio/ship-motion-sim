#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <QApplication>
#include <QByteArray>

#include "app/batch_simulation_runner.h"
#include "app/realtime_simulation_controller.h"
#include "app/simulation_app.h"
#include "app/simulation_session.h"
#include "common/math_utils.h"
#include "common/types.h"
#include "config/xml_config_loader.h"
#include "gui/main_window.h"
#include "logger/state_logger.h"
#include "model/simple_nomoto_ship_model.h"
#include "scenario/command_schedule.h"
#include "scenario/csv_command_io.h"

namespace {

const std::filesystem::path kSourceDir = SHIP_SIM_SOURCE_DIR;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ship_sim::SimulationConfig makeSimulationConfig() {
    return ship_sim::SimulationConfig {0.1, 20.0, 6378137.0, 10.0, 0.08, 8.0, 35.0};
}

ship_sim::SessionConfig makeSessionConfig() {
    ship_sim::SessionConfig config;
    config.simulation = makeSimulationConfig();
    config.initial_state = ship_sim::InitialState {24.0, 120.0, 0.0, 0.0};
    config.engine_panels = {{"default", "Default", 0}};
    config.engine_orders = {{"ahead3", "Ahead 3", "default", 3.0, 0}, {"stop", "Stop", "default", 0.0, 1}};
    config.rudder_presets = {{"midship", "Midship", 0.0, 0}, {"starboard_10", "Starboard 10", 10.0, 1}};
    config.default_output_dir = (std::filesystem::temp_directory_path() / "ship_motion_sim_gui_tests").string();
    config.auto_plot = false;
    config.plot_script_path = (kSourceDir / "scripts" / "plot_simulation.py").string();
    return config;
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open file: " + path.string());
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void requireFileContentEquals(
    const std::filesystem::path& actual_path,
    const std::filesystem::path& expected_path,
    const std::string& context) {
    const std::string actual = readTextFile(actual_path);
    const std::string expected = readTextFile(expected_path);
    if (actual == expected) {
        return;
    }

    std::size_t mismatch_index = 0;
    while (mismatch_index < actual.size() && mismatch_index < expected.size() &&
           actual[mismatch_index] == expected[mismatch_index]) {
        ++mismatch_index;
    }

    throw std::runtime_error(
        context + " baseline mismatch at byte " + std::to_string(mismatch_index) +
        " actual=" + actual_path.string() + " expected=" + expected_path.string());
}

void testSpeedResponse() {
    ship_sim::SimpleNomotoShipModel model(makeSimulationConfig());
    model.setEngineOrderMapping({{"ahead3", 3.0}});
    model.setInitialState(ship_sim::InitialState {24.0, 120.0, 0.0, 0.0});
    model.setEngineCommand("ahead3");

    double previous_speed = 0.0;
    for (int i = 0; i < 100; ++i) {
        model.step(0.1);
        const auto state = model.getState((i + 1) * 0.1);
        require(state.speed_mps >= previous_speed - 1e-9, "speed should increase monotonically");
        require(state.speed_mps <= 3.0 + 1e-9, "speed should not overshoot target for this setup");
        previous_speed = state.speed_mps;
    }
    require(previous_speed > 2.0, "speed should approach target");
}

void testYawResponse() {
    ship_sim::SimpleNomotoShipModel model(makeSimulationConfig());
    model.setEngineOrderMapping({{"ahead3", 3.0}});
    model.setInitialState(ship_sim::InitialState {24.0, 120.0, 0.0, 0.0});
    model.setEngineCommand("ahead3");
    model.setRudderCommandDeg(10.0);

    double previous_heading = 0.0;
    double previous_yaw_rate = 0.0;
    for (int i = 0; i < 50; ++i) {
        model.step(0.1);
        const auto state = model.getState((i + 1) * 0.1);
        require(state.yaw_rate_deg_s >= previous_yaw_rate - 1e-9, "yaw rate should increase monotonically");
        require(state.heading_deg >= previous_heading - 1e-9, "heading should increase under constant starboard rudder");
        previous_heading = state.heading_deg;
        previous_yaw_rate = state.yaw_rate_deg_s;
    }
    require(previous_heading > 0.1, "heading should change noticeably");
}

void testHeadingZeroMovesNorth() {
    ship_sim::SimpleNomotoShipModel model(makeSimulationConfig());
    model.setEngineOrderMapping({{"ahead3", 3.0}});
    model.setInitialState(ship_sim::InitialState {24.0, 120.0, 0.0, 0.0});
    model.setEngineCommand("ahead3");

    ship_sim::ShipState state {};
    for (int i = 0; i < 100; ++i) {
        model.step(0.1);
        state = model.getState((i + 1) * 0.1);
    }

    require(state.lat_deg > 24.0, "heading 0 should move north and increase latitude");
    require(std::abs(state.lon_deg - 120.0) < 1e-4, "heading 0 without rudder should keep longitude nearly unchanged");
}

void testRudderDirectionMatchesTrackDirection() {
    auto run_turn = [](const double rudder_deg) {
        ship_sim::SimpleNomotoShipModel model(makeSimulationConfig());
        model.setEngineOrderMapping({{"ahead3", 3.0}});
        model.setInitialState(ship_sim::InitialState {24.0, 120.0, 0.0, 0.0});
        model.setEngineCommand("ahead3");
        model.setRudderCommandDeg(rudder_deg);

        ship_sim::ShipState state {};
        for (int i = 0; i < 120; ++i) {
            model.step(0.1);
            state = model.getState((i + 1) * 0.1);
        }
        return state;
    };

    const ship_sim::ShipState starboard_state = run_turn(10.0);
    const ship_sim::ShipState port_state = run_turn(-10.0);

    require(starboard_state.heading_deg > 0.1, "starboard rudder should increase navigational heading");
    require(starboard_state.lon_deg > 120.0, "starboard rudder should bend track to the east");
    require(port_state.lon_deg < 120.0, "port rudder should bend track to the west");
}

void testGeoConversion() {
    const auto result = ship_sim::math::localOffsetToLatLonDeg(
        24.0,
        120.0,
        0.0,
        6378137.0 * ship_sim::math::degToRad(1.0),
        6378137.0);
    require(std::abs(result.first - 25.0) < 1e-6, "latitude conversion should match 1 degree north");
    require(std::abs(result.second - 120.0) < 1e-6, "longitude should remain unchanged");
}

void testCommandScheduleOrdering() {
    const auto path = std::filesystem::temp_directory_path() / "ship_motion_sim_commands_test.csv";
    {
        std::ofstream output(path);
        output << "time_s,type,value\n";
        output << "5.0,engine,ahead3\n";
        output << "5.0,rudder,10\n";
        output << "10.0,engine,stop\n";
    }

    auto schedule = ship_sim::CommandSchedule(ship_sim::CsvCommandReader::loadFromFile(path.string()));
    require(schedule.popReadyEvents(0.0).empty(), "no event should be ready at t=0");

    const auto events = schedule.popReadyEvents(5.0);
    require(events.size() == 2, "two events should be ready at t=5");
    require(events[0].type == ship_sim::CommandType::Engine, "stable ordering should preserve CSV order");
    require(events[1].type == ship_sim::CommandType::Rudder, "second event should be rudder");

    std::filesystem::remove(path);
}

void testXmlConfigLoaderAndWriter() {
    const auto temp_dir = std::filesystem::temp_directory_path() / "ship_motion_sim_xml_test";
    std::filesystem::create_directories(temp_dir);
    const auto output_path = temp_dir / "config.xml";

    const ship_sim::SessionConfig source_config = makeSessionConfig();
    ship_sim::XmlConfigWriter::saveToFile(source_config, output_path.string());
    const ship_sim::SessionConfig loaded_config = ship_sim::XmlConfigLoader::loadFromFile(output_path.string());

    require(loaded_config.engine_orders.size() == source_config.engine_orders.size(), "engine orders should round-trip");
    require(loaded_config.rudder_presets.size() == source_config.rudder_presets.size(), "rudder presets should round-trip");
    require(loaded_config.default_output_dir == source_config.default_output_dir, "output dir should round-trip");
}

void testSimulationSessionTracksHistory() {
    ship_sim::SimulationSession session(makeSessionConfig());
    session.applyCommand(ship_sim::CommandEvent {0.0, ship_sim::CommandType::Engine, "ahead3"});
    session.applyCommand(ship_sim::CommandEvent {0.0, ship_sim::CommandType::Rudder, "10"});
    session.step(0.1);
    session.step(0.1);

    require(session.stateHistory().size() == 3, "state history should include initial state and two steps");
    require(session.commandHistory().size() == 2, "command history should include applied commands");
    require(session.currentRudderCommandDeg() == 10.0, "rudder command should be tracked");
    require(session.currentEngineOrderId() == "ahead3", "engine order should be tracked");
}

void testBatchSimulationRunnerWritesArtifacts() {
    const auto temp_dir = std::filesystem::temp_directory_path() / "ship_motion_sim_batch_runner";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    const auto commands_path = temp_dir / "commands.csv";
    {
        std::ofstream output(commands_path);
        output << "time_s,type,value\n";
        output << "0.0,engine,ahead3\n";
    }

    ship_sim::BatchSimulationRunner runner;
    const ship_sim::BatchRunResult result = runner.run(ship_sim::BatchRunOptions {
        makeSessionConfig(),
        commands_path.string(),
        "",
        temp_dir.string(),
        "",
        false,
    });

    require(std::filesystem::exists(result.artifacts.state_csv_path), "state csv should exist");
    require(std::filesystem::exists(result.artifacts.commands_csv_path), "commands csv should exist");
    require(std::filesystem::exists(result.artifacts.summary_path), "summary should exist");
    require(!result.plot_generated, "plot should be disabled");
}

void testSimulationAppWritesOutputFile() {
    const auto output_path = std::filesystem::temp_directory_path() / "ship_motion_sim_output.csv";
    std::filesystem::remove(output_path);

    ship_sim::SimulationApp app;
    const int exit_code = app.run(ship_sim::CliOptions {
        (kSourceDir / "tests" / "fixtures" / "baseline_straight_config.xml").string(),
        (kSourceDir / "tests" / "fixtures" / "baseline_straight_commands.csv").string(),
        output_path.string(),
        "",
        "",
        false,
    });
    require(exit_code == 0, "simulation app should return success");
    require(std::filesystem::exists(output_path), "output csv should be created");
}

void testRealtimeControllerAdvanceByElapsed() {
    ship_sim::SessionConfig config = makeSessionConfig();
    config.simulation.duration_s = 0.2;
    config.default_output_dir = (std::filesystem::temp_directory_path() / "ship_motion_sim_rt").string();

    ship_sim::RealtimeSimulationController controller;
    controller.start(config);
    controller.applyEngineCommand("ahead3");
    controller.advanceByElapsed(0.25);

    require(controller.hasSession(), "controller should own a session after start");
    require(controller.currentState().time_s >= 0.2 - 1e-9, "controller should advance simulation time");
    require(!controller.lastArtifacts().artifacts_dir.empty(), "controller should write artifacts when duration is reached");
}

void testMainWindowSmoke() {
    ship_sim::MainWindow window((kSourceDir / "config" / "default_config.xml").string());
    require(!window.windowTitle().isEmpty(), "main window should initialize");
}

void testEndToEndBaseline(
    const std::string& config_name,
    const std::string& commands_name,
    const std::string& baseline_name) {
    const auto config_path = kSourceDir / "tests" / "fixtures" / config_name;
    const auto commands_path = kSourceDir / "tests" / "fixtures" / commands_name;
    const auto baseline_path = kSourceDir / "tests" / "baselines" / baseline_name;
    const auto output_path = std::filesystem::temp_directory_path() / ("ship_motion_sim_" + baseline_name);

    ship_sim::SimulationApp app;
    const int exit_code = app.run(ship_sim::CliOptions {
        config_path.string(),
        commands_path.string(),
        output_path.string(),
        "",
        "",
        false,
    });
    require(exit_code == 0, "simulation app should succeed for baseline " + baseline_name);
    require(std::filesystem::exists(output_path), "baseline output should exist for " + baseline_name);

    requireFileContentEquals(output_path, baseline_path, baseline_name);
    std::filesystem::remove(output_path);
}

void testEndToEndStraightBaseline() {
    testEndToEndBaseline(
        "baseline_straight_config.xml",
        "baseline_straight_commands.csv",
        "baseline_straight_expected.csv");
}

void testEndToEndTurnBaseline() {
    testEndToEndBaseline(
        "baseline_turn_config.xml",
        "baseline_turn_commands.csv",
        "baseline_turn_expected.csv");
}

}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    try {
        testSpeedResponse();
        testYawResponse();
        testHeadingZeroMovesNorth();
        testRudderDirectionMatchesTrackDirection();
        testGeoConversion();
        testCommandScheduleOrdering();
        testXmlConfigLoaderAndWriter();
        testSimulationSessionTracksHistory();
        testBatchSimulationRunnerWritesArtifacts();
        testSimulationAppWritesOutputFile();
        testRealtimeControllerAdvanceByElapsed();
        testMainWindowSmoke();
        testEndToEndStraightBaseline();
        testEndToEndTurnBaseline();
        std::cout << "All tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failure: " << ex.what() << '\n';
        return 1;
    }
}

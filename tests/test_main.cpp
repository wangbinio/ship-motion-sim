#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QGroupBox>
#include <QPushButton>

#include "app/realtime_simulation_controller.h"
#include "app/simulation_session.h"
#include "common/math_utils.h"
#include "common/types.h"
#include "config/xml_config_loader.h"
#include "gui/main_window.h"
#include "model/simple_nomoto_ship_model.h"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ship_sim::SimulationConfig makeSimulationConfig() {
    return ship_sim::SimulationConfig {0.1, 20.0, 6378137.0, 10.0, 0.08, 8.0, 35.0};
}

std::string defaultConfigPath() {
    const QString path =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../config/default_config.xml"));
    return QDir::cleanPath(path).toStdString();
}

class CurrentPathGuard {
public:
    explicit CurrentPathGuard(const std::filesystem::path& path) : original_(std::filesystem::current_path()) {
        std::filesystem::create_directories(path);
        std::filesystem::current_path(path);
    }

    ~CurrentPathGuard() {
        std::filesystem::current_path(original_);
    }

private:
    std::filesystem::path original_;
};

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open file: " + path.string());
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

QPushButton* findButtonByText(const ship_sim::MainWindow& window, const QString& text) {
    const auto buttons = window.findChildren<QPushButton*>();
    for (QPushButton* button : buttons) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
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

void testXmlConfigLoadDefaultConfig() {
    const ship_sim::SessionConfig config = ship_sim::XmlConfigLoader::loadFromFile(defaultConfigPath());
    require(config.simulation.duration_s == 0.0, "default XML config should run until window close");
    require(config.engine_panels.size() == 2, "default XML config should keep two engine panels");
    require(config.rudder_presets.size() == 7, "default XML config should keep seven rudder presets");
}

void testSimulationSessionTracksCurrentCommands() {
    ship_sim::SimulationSession session(ship_sim::XmlConfigLoader::loadFromFile(defaultConfigPath()));
    session.applyCommand(ship_sim::CommandEvent {0.0, ship_sim::CommandType::Engine, "diesel_ahead_3"});
    session.applyCommand(ship_sim::CommandEvent {0.0, ship_sim::CommandType::Rudder, "10"});
    session.step(0.1);
    session.step(0.1);

    require(std::abs(session.currentTimeS() - 0.2) < 1e-9, "session time should advance");
    require(session.currentRudderCommandDeg() == 10.0, "rudder command should be tracked");
    require(session.currentEngineOrderId() == "diesel_ahead_3", "engine order should be tracked");
    require(std::abs(session.currentState().time_s - 0.2) < 1e-9, "state time should match session time");
}

void testRealtimeControllerLazyStartAndLogFile() {
    const auto temp_root = std::filesystem::temp_directory_path() / "ship_motion_sim_simplify_rt";
    std::filesystem::remove_all(temp_root);
    CurrentPathGuard guard(temp_root / "workspace");

    ship_sim::RealtimeSimulationController controller;
    controller.setSessionConfig(ship_sim::XmlConfigLoader::loadFromFile(defaultConfigPath()));

    require(!controller.hasSession(), "controller should start idle");
    require(!controller.isRunning(), "controller should not run before first command");

    controller.applyEngineCommand("diesel_ahead_3");
    require(controller.hasSession(), "first command should lazily create a session");
    require(controller.isRunning(), "first command should start the timer loop");
    require(!controller.logFilePath().empty(), "controller should expose the created log path");

    controller.advanceByElapsed(0.25);
    require(controller.currentState().time_s >= 0.2 - 1e-9, "controller should advance simulation time");

    const std::filesystem::path log_path = std::filesystem::current_path() / controller.logFilePath();
    require(std::filesystem::exists(log_path), "controller should create a CSV log file");

    const std::string content = readTextFile(log_path);
    require(content.find("time_s,lat_deg,lon_deg,heading_deg,speed_mps,yaw_rate_deg_s") != std::string::npos,
            "log file should contain the CSV header");
    require(content.find("0.200000") != std::string::npos, "log file should contain advanced state rows");

    controller.stop();
    require(!controller.hasSession(), "stop should release the active session");
    require(!controller.isRunning(), "stop should leave controller idle");
}

void testMainWindowSmokeAndSelectionFeedback() {
    const auto temp_root = std::filesystem::temp_directory_path() / "ship_motion_sim_simplify_gui";
    std::filesystem::remove_all(temp_root);
    CurrentPathGuard guard(temp_root / "workspace");

    ship_sim::MainWindow window(defaultConfigPath());
    require(!window.windowTitle().isEmpty(), "main window should initialize");
    require(window.findChildren<QGroupBox*>().size() == 3, "main window should only expose three control groups");
    require(findButtonByText(window, QStringLiteral("开始")) == nullptr, "start button should be removed");
    require(findButtonByText(window, QStringLiteral("停止")) == nullptr, "stop button should be removed");

    QPushButton* rudder_button = findButtonByText(window, QStringLiteral("右10"));
    QPushButton* engine_button = findButtonByText(window, QStringLiteral("两进三"));
    require(rudder_button != nullptr, "rudder button should exist");
    require(engine_button != nullptr, "engine button should exist");

    engine_button->click();
    rudder_button->click();

    require(engine_button->isChecked(), "engine button should keep current selection feedback");
    require(rudder_button->isChecked(), "rudder button should keep current selection feedback");

    window.close();
}

}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    try {
        testSpeedResponse();
        testYawResponse();
        testGeoConversion();
        testXmlConfigLoadDefaultConfig();
        testSimulationSessionTracksCurrentCommands();
        testRealtimeControllerLazyStartAndLogFile();
        testMainWindowSmokeAndSelectionFeedback();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failure: " << ex.what() << '\n';
        return 1;
    }
}

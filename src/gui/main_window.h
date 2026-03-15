#pragma once

#include <string>
#include <unordered_map>

#include <QMainWindow>

#include "app/realtime_simulation_controller.h"
#include "common/types.h"

class QButtonGroup;
class QPushButton;
class QVBoxLayout;

namespace ship_sim {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const std::string& config_path, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void handleStateAdvanced();
    void handleError(const QString& message);

private:
    void buildUi();
    void rebuildControlPanels();
    void updateActiveCommandButtons();
    void clearButtonGroupSelection(QButtonGroup* group);

    SessionConfig session_config_;
    RealtimeSimulationController* controller_ {nullptr};
    QVBoxLayout* control_panels_layout_ {nullptr};
    QButtonGroup* rudder_button_group_ {nullptr};
    QButtonGroup* engine_button_group_ {nullptr};
    std::unordered_map<std::string, QPushButton*> engine_buttons_by_id_;
    std::unordered_map<int, QPushButton*> rudder_buttons_by_key_;
};

}  // namespace ship_sim

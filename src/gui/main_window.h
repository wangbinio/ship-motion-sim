#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <QMainWindow>

#include "app/realtime_simulation_controller.h"
#include "common/types.h"

class QDoubleSpinBox;
class QButtonGroup;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QVBoxLayout;
class QWidget;

namespace ship_sim {

class TrackViewWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const std::string& config_path, QWidget* parent = nullptr);

private slots:
    void handleLoadConfig();
    void handleSaveConfig();
    void handleStart();
    void handleStop();
    void handleStateAdvanced();
    void handleRunningChanged(bool running);
    void handleSessionFinished(const QString& artifacts_dir);
    void handleError(const QString& message);

private:
    void buildUi();
    void loadConfigFile(const std::string& path);
    void applyConfigToInputs();
    SessionConfig currentConfigFromInputs() const;
    void rebuildControlPanels();
    void setRunningState(bool running);
    void appendLogLine(const QString& line);
    QString currentEngineLabel() const;
    void updateActiveCommandButtons();
    void clearButtonGroupSelection(QButtonGroup* group);

    std::string current_config_path_;
    SessionConfig session_config_;
    RealtimeSimulationController* controller_ {nullptr};

    QDoubleSpinBox* lat_spin_ {nullptr};
    QDoubleSpinBox* lon_spin_ {nullptr};
    QDoubleSpinBox* heading_spin_ {nullptr};
    QDoubleSpinBox* speed_spin_ {nullptr};

    QLabel* time_value_ {nullptr};
    QLabel* lat_value_ {nullptr};
    QLabel* lon_value_ {nullptr};
    QLabel* heading_value_ {nullptr};
    QLabel* speed_value_ {nullptr};
    QLabel* yaw_value_ {nullptr};
    QLabel* rudder_value_ {nullptr};
    QLabel* engine_value_ {nullptr};
    QLabel* output_dir_value_ {nullptr};

    QPushButton* load_button_ {nullptr};
    QPushButton* save_button_ {nullptr};
    QPushButton* start_button_ {nullptr};
    QPushButton* stop_button_ {nullptr};
    QPlainTextEdit* event_log_ {nullptr};
    TrackViewWidget* track_view_ {nullptr};
    QVBoxLayout* control_panels_layout_ {nullptr};
    QButtonGroup* rudder_button_group_ {nullptr};
    QButtonGroup* engine_button_group_ {nullptr};

    std::unordered_map<std::string, std::string> engine_labels_by_id_;
    std::unordered_map<std::string, QPushButton*> engine_buttons_by_id_;
    std::unordered_map<int, QPushButton*> rudder_buttons_by_key_;
    std::vector<QWidget*> runtime_control_widgets_;
};

}  // namespace ship_sim

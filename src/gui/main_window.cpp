#include "gui/main_window.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <QButtonGroup>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "config/xml_config_loader.h"
#include "gui/track_view_widget.h"

namespace ship_sim {

namespace {

SessionConfig makeFallbackSessionConfig() {
    SessionConfig config;
    config.simulation = SimulationConfig {0.1, 180.0, 6378137.0, 10.0, 0.08, 8.0, 35.0};
    config.initial_state = InitialState {24.0, 120.0, 0.0, 0.0};
    config.engine_panels = {
        {"diesel", "柴油机车令", 0},
        {"fuel", "燃油机车令", 1},
    };
    config.engine_orders = {
        {"diesel_ahead_1", "两进一", "diesel", 1.0, 0},
        {"diesel_ahead_2", "两进二", "diesel", 2.0, 1},
        {"diesel_ahead_3", "两进三", "diesel", 3.0, 2},
        {"diesel_ahead_4", "两进四", "diesel", 4.0, 3},
        {"diesel_ahead_5", "两进五", "diesel", 5.0, 4},
        {"diesel_ahead_6", "两进六", "diesel", 6.0, 5},
        {"diesel_full", "全速", "diesel", 6.5, 6},
        {"diesel_astern_3", "两退三", "diesel", -1.5, 7},
        {"diesel_astern_4", "两退四", "diesel", -2.0, 8},
        {"diesel_stop", "停车", "diesel", 0.0, 9},
        {"fuel_ahead_6", "两进六", "fuel", 6.0, 0},
        {"fuel_ahead_7", "两进七", "fuel", 7.0, 1},
        {"fuel_ahead_8", "两进八", "fuel", 8.0, 2},
        {"fuel_ahead_9", "两进九", "fuel", 9.0, 3},
        {"fuel_ahead_10", "两进十", "fuel", 10.0, 4},
        {"fuel_full", "全速", "fuel", 10.5, 5},
        {"fuel_astern_4", "两退四", "fuel", -2.0, 6},
        {"fuel_astern_5", "两退五", "fuel", -2.5, 7},
        {"fuel_stop", "停车", "fuel", 0.0, 8},
    };
    config.rudder_presets = {
        {"port_30", "左30", -30.0, 0},
        {"port_10", "左10", -10.0, 1},
        {"port_5", "左5", -5.0, 2},
        {"midship", "舵正", 0.0, 3},
        {"starboard_5", "右5", 5.0, 4},
        {"starboard_10", "右10", 10.0, 5},
        {"starboard_30", "右30", 30.0, 6},
    };
    config.default_output_dir = "output/runs";
    config.auto_plot = true;
    config.plot_script_path = "scripts/plot_simulation.py";
    return config;
}

void clearLayout(QLayout* layout) {
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->layout() != nullptr) {
            clearLayout(item->layout());
        }
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

QPushButton* makePresetButton(const QString& text) {
    QPushButton* button = new QPushButton(text);
    button->setMinimumHeight(36);
    button->setCheckable(true);
    button->setStyleSheet(
        "QPushButton {"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "  background: #f8fafc;"
        "}"
        "QPushButton:checked {"
        "  background: #2563eb;"
        "  color: white;"
        "  border-color: #1d4ed8;"
        "  font-weight: 600;"
        "}"
        "QPushButton:disabled {"
        "  color: #94a3b8;"
        "  background: #e2e8f0;"
        "}");
    return button;
}

int rudderButtonKey(const double rudder_deg) {
    return static_cast<int>(std::round(rudder_deg * 10.0));
}

}  // namespace

MainWindow::MainWindow(const std::string& config_path, QWidget* parent) : QMainWindow(parent) {
    controller_ = new RealtimeSimulationController(this);
    buildUi();

    connect(load_button_, &QPushButton::clicked, this, &MainWindow::handleLoadConfig);
    connect(save_button_, &QPushButton::clicked, this, &MainWindow::handleSaveConfig);
    connect(start_button_, &QPushButton::clicked, this, &MainWindow::handleStart);
    connect(stop_button_, &QPushButton::clicked, this, &MainWindow::handleStop);
    connect(controller_, &RealtimeSimulationController::stateAdvanced, this, &MainWindow::handleStateAdvanced);
    connect(controller_, &RealtimeSimulationController::runningChanged, this, &MainWindow::handleRunningChanged);
    connect(controller_, &RealtimeSimulationController::sessionFinished, this, &MainWindow::handleSessionFinished);
    connect(controller_, &RealtimeSimulationController::errorOccurred, this, &MainWindow::handleError);

    try {
        loadConfigFile(config_path.empty() ? "config/default_config.xml" : config_path);
    } catch (const std::exception& ex) {
        session_config_ = makeFallbackSessionConfig();
        applyConfigToInputs();
        rebuildControlPanels();
        appendLogLine(QStringLiteral("加载默认 XML 失败，已回退到内置配置: ") + QString::fromStdString(ex.what()));
    }

    setWindowTitle(QStringLiteral("船舶机动仿真"));
    resize(1280, 860);
    setRunningState(false);
}

void MainWindow::handleLoadConfig() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Load XML Config"),
        QString::fromStdString(current_config_path_.empty() ? "config" : current_config_path_),
        tr("XML Files (*.xml)"));
    if (path.isEmpty()) {
        return;
    }

    try {
        loadConfigFile(path.toStdString());
        appendLogLine(tr("已加载配置: %1").arg(path));
    } catch (const std::exception& ex) {
        handleError(QString::fromStdString(ex.what()));
    }
}

void MainWindow::handleSaveConfig() {
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save XML Config"),
        QString::fromStdString(current_config_path_.empty() ? "config/default_config.xml" : current_config_path_),
        tr("XML Files (*.xml)"));
    if (path.isEmpty()) {
        return;
    }

    try {
        XmlConfigWriter::saveToFile(currentConfigFromInputs(), path.toStdString());
        current_config_path_ = path.toStdString();
        appendLogLine(tr("已保存配置: %1").arg(path));
    } catch (const std::exception& ex) {
        handleError(QString::fromStdString(ex.what()));
    }
}

void MainWindow::handleStart() {
    try {
        event_log_->clear();
        track_view_->setTrackData({}, {});
        controller_->start(currentConfigFromInputs());
        updateActiveCommandButtons();
        appendLogLine(QStringLiteral("仿真已开始"));
    } catch (const std::exception& ex) {
        handleError(QString::fromStdString(ex.what()));
    }
}

void MainWindow::handleStop() {
    controller_->stop();
}

void MainWindow::handleStateAdvanced() {
    if (!controller_->hasSession()) {
        return;
    }

    const ShipState& state = controller_->currentState();
    time_value_->setText(QString::number(state.time_s, 'f', 2));
    lat_value_->setText(QString::number(state.lat_deg, 'f', 6));
    lon_value_->setText(QString::number(state.lon_deg, 'f', 6));
    heading_value_->setText(QString::number(state.heading_deg, 'f', 3));
    speed_value_->setText(QString::number(state.speed_mps, 'f', 3));
    yaw_value_->setText(QString::number(state.yaw_rate_deg_s, 'f', 3));
    rudder_value_->setText(QString::number(controller_->currentRudderCommandDeg(), 'f', 1));
    engine_value_->setText(currentEngineLabel());
    updateActiveCommandButtons();
    track_view_->setTrackData(controller_->stateHistory(), controller_->commandHistory());
}

void MainWindow::handleRunningChanged(const bool running) {
    setRunningState(running);
}

void MainWindow::handleSessionFinished(const QString& artifacts_dir) {
    output_dir_value_->setText(artifacts_dir);
    appendLogLine(tr("仿真已停止，产物目录: %1").arg(artifacts_dir));
}

void MainWindow::handleError(const QString& message) {
    appendLogLine(QStringLiteral("错误: ") + message);
    QMessageBox::warning(this, tr("Error"), message);
}

void MainWindow::buildUi() {
    QWidget* central = new QWidget(this);
    QVBoxLayout* root_layout = new QVBoxLayout(central);

    QHBoxLayout* toolbar_layout = new QHBoxLayout();
    load_button_ = new QPushButton(tr("加载 XML"));
    save_button_ = new QPushButton(tr("保存 XML"));
    start_button_ = new QPushButton(tr("开始"));
    stop_button_ = new QPushButton(tr("停止"));
    toolbar_layout->addWidget(load_button_);
    toolbar_layout->addWidget(save_button_);
    toolbar_layout->addStretch(1);
    toolbar_layout->addWidget(start_button_);
    toolbar_layout->addWidget(stop_button_);
    root_layout->addLayout(toolbar_layout);

    QHBoxLayout* top_layout = new QHBoxLayout();

    QGroupBox* input_group = new QGroupBox(tr("初始状态"));
    QFormLayout* input_form = new QFormLayout(input_group);
    lat_spin_ = new QDoubleSpinBox();
    lat_spin_->setRange(-89.0, 89.0);
    lat_spin_->setDecimals(6);
    lon_spin_ = new QDoubleSpinBox();
    lon_spin_->setRange(-180.0, 180.0);
    lon_spin_->setDecimals(6);
    heading_spin_ = new QDoubleSpinBox();
    heading_spin_->setRange(0.0, 360.0);
    heading_spin_->setDecimals(3);
    speed_spin_ = new QDoubleSpinBox();
    speed_spin_->setRange(-50.0, 50.0);
    speed_spin_->setDecimals(3);
    input_form->addRow(tr("纬度 (deg)"), lat_spin_);
    input_form->addRow(tr("经度 (deg)"), lon_spin_);
    input_form->addRow(tr("航向 (deg)"), heading_spin_);
    input_form->addRow(tr("航速 (m/s)"), speed_spin_);
    top_layout->addWidget(input_group, 1);

    QGroupBox* status_group = new QGroupBox(tr("实时状态"));
    QFormLayout* status_form = new QFormLayout(status_group);
    time_value_ = new QLabel("-");
    lat_value_ = new QLabel("-");
    lon_value_ = new QLabel("-");
    heading_value_ = new QLabel("-");
    speed_value_ = new QLabel("-");
    yaw_value_ = new QLabel("-");
    rudder_value_ = new QLabel("-");
    engine_value_ = new QLabel("-");
    output_dir_value_ = new QLabel("-");
    status_form->addRow(tr("仿真时间 (s)"), time_value_);
    status_form->addRow(tr("纬度"), lat_value_);
    status_form->addRow(tr("经度"), lon_value_);
    status_form->addRow(tr("航向 (deg)"), heading_value_);
    status_form->addRow(tr("航速 (m/s)"), speed_value_);
    status_form->addRow(tr("角速度 (deg/s)"), yaw_value_);
    status_form->addRow(tr("当前舵令"), rudder_value_);
    status_form->addRow(tr("当前车令"), engine_value_);
    status_form->addRow(tr("输出目录"), output_dir_value_);
    top_layout->addWidget(status_group, 1);

    event_log_ = new QPlainTextEdit();
    event_log_->setReadOnly(true);
    event_log_->setMinimumWidth(280);
    top_layout->addWidget(event_log_, 1);
    root_layout->addLayout(top_layout);

    track_view_ = new TrackViewWidget();
    root_layout->addWidget(track_view_, 1);

    QWidget* control_widget = new QWidget();
    control_panels_layout_ = new QVBoxLayout(control_widget);
    control_panels_layout_->setContentsMargins(0, 0, 0, 0);

    QScrollArea* control_scroll = new QScrollArea();
    control_scroll->setWidgetResizable(true);
    control_scroll->setWidget(control_widget);
    root_layout->addWidget(control_scroll, 0);

    setCentralWidget(central);
}

void MainWindow::loadConfigFile(const std::string& path) {
    session_config_ = XmlConfigLoader::loadFromFile(path);
    current_config_path_ = path;
    applyConfigToInputs();
    rebuildControlPanels();
}

void MainWindow::applyConfigToInputs() {
    lat_spin_->setValue(session_config_.initial_state.lat_deg);
    lon_spin_->setValue(session_config_.initial_state.lon_deg);
    heading_spin_->setValue(session_config_.initial_state.heading_deg);
    speed_spin_->setValue(session_config_.initial_state.speed_mps);
    output_dir_value_->setText(QString::fromStdString(session_config_.default_output_dir));
}

SessionConfig MainWindow::currentConfigFromInputs() const {
    SessionConfig config = session_config_;
    config.initial_state.lat_deg = lat_spin_->value();
    config.initial_state.lon_deg = lon_spin_->value();
    config.initial_state.heading_deg = heading_spin_->value();
    config.initial_state.speed_mps = speed_spin_->value();
    return config;
}

void MainWindow::rebuildControlPanels() {
    clearLayout(control_panels_layout_);
    runtime_control_widgets_.clear();
    engine_labels_by_id_.clear();
    engine_buttons_by_id_.clear();
    rudder_buttons_by_key_.clear();
    delete rudder_button_group_;
    delete engine_button_group_;
    rudder_button_group_ = new QButtonGroup(this);
    engine_button_group_ = new QButtonGroup(this);
    rudder_button_group_->setExclusive(true);
    engine_button_group_->setExclusive(true);

    std::vector<RudderPresetDefinition> rudder_presets = session_config_.rudder_presets;
    std::sort(
        rudder_presets.begin(),
        rudder_presets.end(),
        [](const RudderPresetDefinition& lhs, const RudderPresetDefinition& rhs) {
            return lhs.display_order < rhs.display_order;
        });

    QGroupBox* rudder_group = new QGroupBox(tr("舵令"));
    QHBoxLayout* rudder_layout = new QHBoxLayout(rudder_group);
    for (const auto& preset : rudder_presets) {
        QPushButton* button = makePresetButton(QString::fromStdString(preset.label));
        rudder_button_group_->addButton(button);
        rudder_buttons_by_key_[rudderButtonKey(preset.angle_deg)] = button;
        connect(button, &QPushButton::clicked, this, [this, preset]() {
            controller_->applyRudderCommand(preset.angle_deg);
            appendLogLine(tr("下发舵令: %1").arg(QString::fromStdString(preset.label)));
            updateActiveCommandButtons();
        });
        rudder_layout->addWidget(button);
        runtime_control_widgets_.push_back(button);
    }
    control_panels_layout_->addWidget(rudder_group);

    std::vector<EnginePanelDefinition> engine_panels = session_config_.engine_panels;
    std::sort(
        engine_panels.begin(),
        engine_panels.end(),
        [](const EnginePanelDefinition& lhs, const EnginePanelDefinition& rhs) {
            return lhs.display_order < rhs.display_order;
        });

    for (const auto& order : session_config_.engine_orders) {
        engine_labels_by_id_[order.id] = order.label;
    }

    for (const auto& panel : engine_panels) {
        std::vector<EngineOrderDefinition> panel_orders;
        for (const auto& order : session_config_.engine_orders) {
            if (order.panel_id == panel.id) {
                panel_orders.push_back(order);
            }
        }
        std::sort(
            panel_orders.begin(),
            panel_orders.end(),
            [](const EngineOrderDefinition& lhs, const EngineOrderDefinition& rhs) {
                return lhs.display_order < rhs.display_order;
            });

        QGroupBox* panel_group = new QGroupBox(QString::fromStdString(panel.label));
        QGridLayout* panel_layout = new QGridLayout(panel_group);
        int row = 0;
        int column = 0;
        for (const auto& order : panel_orders) {
            QPushButton* button = makePresetButton(QString::fromStdString(order.label));
            engine_button_group_->addButton(button);
            engine_buttons_by_id_[order.id] = button;
            connect(button, &QPushButton::clicked, this, [this, order]() {
                controller_->applyEngineCommand(order.id);
                appendLogLine(tr("下发车令: %1").arg(QString::fromStdString(order.label)));
                updateActiveCommandButtons();
            });
            panel_layout->addWidget(button, row, column);
            runtime_control_widgets_.push_back(button);
            ++column;
            if (column >= 5) {
                column = 0;
                ++row;
            }
        }
        control_panels_layout_->addWidget(panel_group);
    }

    control_panels_layout_->addStretch(1);
    updateActiveCommandButtons();
}

void MainWindow::setRunningState(const bool running) {
    lat_spin_->setEnabled(!running);
    lon_spin_->setEnabled(!running);
    heading_spin_->setEnabled(!running);
    speed_spin_->setEnabled(!running);
    load_button_->setEnabled(!running);
    save_button_->setEnabled(!running);
    start_button_->setEnabled(!running);
    stop_button_->setEnabled(running);
    for (QWidget* widget : runtime_control_widgets_) {
        widget->setEnabled(running);
    }
}

void MainWindow::appendLogLine(const QString& line) {
    event_log_->appendPlainText(line);
}

QString MainWindow::currentEngineLabel() const {
    const auto it = engine_labels_by_id_.find(controller_->currentEngineOrderId());
    if (it == engine_labels_by_id_.end()) {
        return QStringLiteral("-");
    }
    return QString::fromStdString(it->second);
}

void MainWindow::updateActiveCommandButtons() {
    if (rudder_button_group_ == nullptr || engine_button_group_ == nullptr) {
        return;
    }

    clearButtonGroupSelection(rudder_button_group_);
    clearButtonGroupSelection(engine_button_group_);

    if (!controller_->hasSession()) {
        return;
    }

    const auto rudder_it = rudder_buttons_by_key_.find(rudderButtonKey(controller_->currentRudderCommandDeg()));
    if (rudder_it != rudder_buttons_by_key_.end()) {
        rudder_it->second->setChecked(true);
    }

    const auto engine_it = engine_buttons_by_id_.find(controller_->currentEngineOrderId());
    if (engine_it != engine_buttons_by_id_.end()) {
        engine_it->second->setChecked(true);
    }
}

void MainWindow::clearButtonGroupSelection(QButtonGroup* group) {
    if (group == nullptr) {
        return;
    }

    const bool was_exclusive = group->exclusive();
    group->setExclusive(false);
    for (QAbstractButton* button : group->buttons()) {
        button->setChecked(false);
    }
    group->setExclusive(was_exclusive);
}

}  // namespace ship_sim

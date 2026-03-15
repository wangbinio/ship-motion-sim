#include "gui/main_window.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <QButtonGroup>
#include <QCloseEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "config/xml_config_loader.h"

namespace ship_sim {

namespace {

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
    button->setMinimumHeight(42);
    button->setCheckable(true);
    button->setStyleSheet(
        "QPushButton {"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 8px 14px;"
        "  background: #f8fafc;"
        "}"
        "QPushButton:checked {"
        "  background: #2563eb;"
        "  color: white;"
        "  border-color: #1d4ed8;"
        "  font-weight: 600;"
        "}");
    return button;
}

int rudderButtonKey(const double rudder_deg) {
    return static_cast<int>(std::round(rudder_deg * 10.0));
}

}  // namespace

MainWindow::MainWindow(const std::string& config_path, QWidget* parent) : QMainWindow(parent) {
    if (config_path.empty()) {
        throw std::runtime_error("Config path must not be empty");
    }

    session_config_ = XmlConfigLoader::loadFromFile(config_path);
    controller_ = new RealtimeSimulationController(this);
    controller_->setSessionConfig(session_config_);

    buildUi();
    rebuildControlPanels();

    connect(controller_, &RealtimeSimulationController::stateAdvanced, this, &MainWindow::handleStateAdvanced);
    connect(controller_, &RealtimeSimulationController::errorOccurred, this, &MainWindow::handleError);

    setWindowTitle(QStringLiteral("船舶机动仿真"));
    resize(960, 520);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    controller_->stop();
    QMainWindow::closeEvent(event);
}

void MainWindow::handleStateAdvanced() {
    updateActiveCommandButtons();
}

void MainWindow::handleError(const QString& message) {
    QMessageBox::warning(this, tr("Error"), message);
}

void MainWindow::buildUi() {
    QWidget* central = new QWidget(this);
    control_panels_layout_ = new QVBoxLayout(central);
    control_panels_layout_->setContentsMargins(24, 24, 24, 24);
    control_panels_layout_->setSpacing(18);
    setCentralWidget(central);
}

void MainWindow::rebuildControlPanels() {
    clearLayout(control_panels_layout_);
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
    rudder_layout->setSpacing(10);
    for (const auto& preset : rudder_presets) {
        QPushButton* button = makePresetButton(QString::fromStdString(preset.label));
        rudder_button_group_->addButton(button);
        rudder_buttons_by_key_[rudderButtonKey(preset.angle_deg)] = button;
        connect(button, &QPushButton::clicked, this, [this, preset]() {
            controller_->applyRudderCommand(preset.angle_deg);
        });
        rudder_layout->addWidget(button);
    }
    control_panels_layout_->addWidget(rudder_group);

    std::vector<EnginePanelDefinition> engine_panels = session_config_.engine_panels;
    std::sort(
        engine_panels.begin(),
        engine_panels.end(),
        [](const EnginePanelDefinition& lhs, const EnginePanelDefinition& rhs) {
            return lhs.display_order < rhs.display_order;
        });

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
        panel_layout->setHorizontalSpacing(10);
        panel_layout->setVerticalSpacing(10);

        int row = 0;
        int column = 0;
        for (const auto& order : panel_orders) {
            QPushButton* button = makePresetButton(QString::fromStdString(order.label));
            engine_button_group_->addButton(button);
            engine_buttons_by_id_[order.id] = button;
            connect(button, &QPushButton::clicked, this, [this, order]() {
                controller_->applyEngineCommand(order.id);
            });
            panel_layout->addWidget(button, row, column);
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

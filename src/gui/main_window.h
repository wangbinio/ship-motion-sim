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
    // 使用给定配置路径初始化主窗口和实时控制器。
    explicit MainWindow(const std::string& config_path, QWidget* parent = nullptr);

protected:
    // 关闭窗口时停止实时控制器，避免后台继续输出日志。
    void closeEvent(QCloseEvent* event) override;

private slots:
    // 在实时状态推进后同步按钮选中态。
    void handleStateAdvanced();
    // 统一处理控制器上报的错误信息。
    void handleError(const QString& message);

private:
    // 构建主窗口的基础布局容器。
    void buildUi();
    // 应用窗口级显示属性，例如置顶、按钮显隐和透明背景。
    void applyWindowAppearance();
    // 从资源文件加载并应用独立 QSS。
    void applyStyleSheet();
    // 按配置重新生成舵令和车令控制面板。
    void rebuildControlPanels();
    // 根据当前控制器状态刷新按钮选中态。
    void updateActiveCommandButtons();
    // 清除按钮组的现有选中项。
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

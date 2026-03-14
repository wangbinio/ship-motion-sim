# Ship Motion Sim Architecture

## 1. 文档目的

本文档描述当前仓库在阶段 2 实施后的实际架构状态，重点回答：

- 系统现在有哪些可执行入口
- 仿真内核、配置、GUI 和分析链路如何分层
- 数据与控制如何在系统内流动

本文档描述的是当前实现，不是未来理想态。

## 2. 当前系统定位

当前项目是一个单舰二维平面机动仿真原型，特点是：

- 核心数值模型保持简单
- 既支持 GUI 实时交互，也支持 CLI 批量回放
- 统一输出分析产物
- 继续强调可验证性而非高保真

它仍然不是通用仿真平台，当前最准确的定义是：

“一个可交互、可回放、可出图的舰艇机动仿真原型工程”。

## 3. 入口与产物

当前 CMake 生成 4 个主要二进制：

- `ship_motion_sim`
- `ship_motion_sim_gui`
- `ship_motion_sim_tests`

其中：

- `ship_motion_sim` 是 CLI 入口
- `ship_motion_sim_gui` 是 Qt Widgets 图形界面
- `ship_motion_sim_tests` 是统一测试入口

标准分析产物为：

- `state.csv`
- `commands.csv`
- `summary.txt`
- `plot.png`

## 4. 模块分层

当前代码可以概括为 6 层。

### 4.1 入口层

文件：

- `src/main.cpp`
- `src/gui_main.cpp`

职责：

- 创建 CLI 或 GUI 应用
- 解析最外层参数
- 捕获异常并转成退出码

### 4.2 应用编排层

文件：

- `src/app/simulation_app.*`
- `src/app/batch_simulation_runner.*`
- `src/app/realtime_simulation_controller.*`
- `src/app/plot_runner.*`
- `src/app/simulation_session.*`

职责：

- 统一管理一次仿真会话
- 为 CLI 驱动批量运行
- 为 GUI 驱动实时运行
- 导出产物
- 调用 Python 绘图脚本

这里是阶段 2 最大的新增层。

### 4.3 领域模型层

文件：

- `src/model/simple_nomoto_ship_model.*`

职责：

- 保存船舶内部状态
- 响应当前控制量
- 推进一步
- 输出对外状态快照

这一层仍然是数值核心，阶段 2 没有推翻它。

### 4.4 输入解析与调度层

文件：

- `src/config/xml_config_loader.*`
- `src/scenario/csv_command_io.*`
- `src/scenario/command_schedule.*`

职责：

- 解析 XML 配置
- 解析与写回命令 CSV
- 按仿真时间调度离散命令

### 4.5 基础设施层

文件：

- `src/logger/state_logger.*`
- `src/common/*`

职责：

- 写状态 CSV
- 保存公共数据结构
- 提供角度与经纬度换算工具

### 4.6 表现层

文件：

- `src/gui/main_window.*`
- `src/gui/track_view_widget.*`

职责：

- 呈现 Qt 主界面
- 显示当前状态
- 构建舵令与车令面板
- 绘制实时轨迹图

## 5. 核心对象关系

当前依赖关系如下：

```text
CLI Main
  -> SimulationApp
      -> XmlConfigLoader
      -> BatchSimulationRunner
          -> CsvCommandReader
          -> CommandSchedule
          -> SimulationSession
              -> SimpleNomotoShipModel
          -> StateLogger
          -> PlotRunner

GUI Main
  -> MainWindow
      -> RealtimeSimulationController
          -> SimulationSession
              -> SimpleNomotoShipModel
          -> StateLogger
          -> CsvCommandWriter
          -> PlotRunner
      -> TrackViewWidget
```

这个依赖方向是健康的：

- GUI 不直接操作模型
- CLI 不再自己管理时间推进细节
- 模型仍然不依赖 Qt Widgets

## 6. 核心数据模型

阶段 2 后，公共数据结构主要分为 4 类。

### 6.1 仿真参数

- `SimulationConfig`
- `InitialState`
- `ShipState`

### 6.2 命令数据

- `CommandType`
- `CommandEvent`
- `CommandEvents`

### 6.3 控制面板定义

- `EnginePanelDefinition`
- `EngineOrderDefinition`
- `RudderPresetDefinition`

这组结构是阶段 2 为 GUI 引入的关键抽象，用来表达 `control.png` 中的命令布局。

### 6.4 会话与产物定义

- `SessionConfig`
- `ArtifactPaths`
- `BatchRunOptions`
- `BatchRunResult`

## 7. 会话层设计

`SimulationSession` 是阶段 2 的核心中枢。

它负责：

- 持有 `SimpleNomotoShipModel`
- 维护当前仿真时刻
- 应用舵令/车令
- 记录状态历史
- 记录命令历史

这意味着：

- CLI 与 GUI 共用一套会话语义
- 端到端测试也直接围绕同一对象建模

## 8. CLI 数据流

当前 CLI 批量回放的数据流为：

```text
XML Config + Command CSV
  -> XmlConfigLoader / CsvCommandReader
  -> BatchSimulationRunner
  -> CommandSchedule
  -> SimulationSession.step(dt)
  -> StateLogger
  -> commands.csv / summary.txt / plot.png
```

特点：

- 保留离散事件控制语义
- 保留固定步长积分
- 新增统一产物目录能力

## 9. GUI 数据流

当前 GUI 实时交互的数据流为：

```text
MainWindow
  -> 用户点击开始
  -> RealtimeSimulationController.start(config)
  -> QTimer + QElapsedTimer
  -> SimulationSession.step(dt)
  -> stateAdvanced signal
  -> MainWindow 更新标签和轨迹图
  -> 用户点击舵令/车令按钮
  -> controller.applyCommand(...)
  -> 命令立即生效并记录历史
  -> 停止后导出产物并可选自动绘图
```

这里采用的是：

- 单线程
- 固定积分步长
- 墙钟时间累积器

这样避免了把 UI 刷新频率和模型步长直接绑定。

## 10. 配置系统

阶段 2 已从 JSON 迁到 XML。

当前 XML 配置承载：

- 仿真参数
- 模型参数
- 初始状态
- 分析输出默认值
- 车令面板定义
- 车令定义
- 舵令预设

运行时不再依赖自定义 JSON 解析器。

## 11. 分析链路

阶段 2 之前，绘图脚本是手工调用的离线工具。

阶段 2 之后：

- CLI 可自动调用绘图脚本
- GUI 停止后也可自动调用同一脚本
- 两条路径共享同一产物协议

`PlotRunner` 当前通过 `QProcess` 调起：

```text
python3 scripts/plot_simulation.py --input state.csv --commands commands.csv --output plot.png
```

这让 GUI 与 CLI 的结果图保持一致。

## 12. 测试结构

当前测试仍集中在 `tests/test_main.cpp`，但覆盖范围已经扩展到：

- 模型行为
- CSV 命令读取与调度
- XML 配置读写
- 会话历史记录
- 批量产物生成
- CLI 输出
- 实时控制器推进
- GUI 构造烟雾测试
- 两组端到端基线

虽然测试文件尚未拆分，但验证边界已经覆盖到阶段 2 的核心路径。

## 13. 当前架构优点

### 13.1 内核复用成功

GUI 与 CLI 没有分叉出两套模型逻辑，`SimulationSession` 让数值语义保持一致。

### 13.2 输入适配层清晰

XML 配置、CSV 命令、状态 CSV、绘图脚本现在边界更明确，文件协议比阶段 1 更统一。

### 13.3 GUI 依赖被限制在表现层

Qt Widgets 没有侵入 `SimpleNomotoShipModel`，测试仍可以在不启动完整 GUI 的情况下验证主要行为。

## 14. 当前不足

### 14.1 控制面板样式仍然是工程化复刻

当前 GUI 已复刻 `control.png` 的命令结构，但没有逐像素复刻原始视觉样式。

### 14.2 车令速度仍是约定值

图片只定义了车令结构，没有给出真实标定速度，因此 XML 中的 `target_speed_mps` 目前仍是工程默认值。

### 14.3 测试文件尚未模块化

测试覆盖面已经变宽，但 `tests/test_main.cpp` 仍是单文件，后续继续扩展时应拆分。

## 15. 后续建议

在当前架构基础上，下一步更适合继续做：

1. 把测试按 `config / session / batch / gui` 拆分
2. 为 GUI 增加更明确的运行状态和命令历史面板
3. 为 `summary.txt` 增加更完整的会话元数据
4. 如果需要更高真实性，再引入舵机动态或速度相关参数

## 16. 总结

阶段 2 完成后的架构可以概括为：

- 一个保持简洁的仿真内核
- 一个统一的会话层连接 CLI 和 GUI
- 一个 XML + CSV 的输入输出适配层
- 一个可自动出图的分析链路
- 一个基于 Qt Widgets 的实时操作界面

它已经从“只能离线回放的 CLI 原型”演进为“可实时交互、可批量分析、可验证回归”的工程原型。 

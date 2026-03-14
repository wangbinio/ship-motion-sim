# Ship Motion Sim 阶段 2 技术方案

## 1. 文档目标

本文档基于以下现状材料生成：

- `phase-2-draft.md`
- `architecture.md`
- `README.md`
- `final.md`
- 当前 `src/`、`scripts/`、`tests/` 实现

目标不是重新定义数学模型，而是在当前可运行 CLI 原型基础上，给出阶段 2 的落地方案，使系统同时具备：

- Qt 图形界面
- XML 配置加载与配置编辑
- 实时交互式单舰仿真
- 保留现有 CLI 批量回放分析能力
- 一次命令产生日志与图像分析结果

本文档默认阶段 2 仍然坚持当前物理边界：单舰、二维平面、一阶 Nomoto 航向模型、一阶速度响应模型，不引入 3-DOF、MMG 或环境扰动。

## 2. 当前系统现状与约束

### 2.1 已有能力

结合 `architecture.md` 与源码，当前系统已经具备完整的离线闭环：

- `SimpleNomotoShipModel` 负责数值推进
- `CommandSchedule` 负责 CSV 事件排序与按时出队
- `StateLogger` 负责状态 CSV 输出
- `SimulationApp` 负责 CLI 装配与主循环
- `plot_simulation.py` 负责离线绘图
- `tests/test_main.cpp` 已覆盖模型、配置、命令调度、日志输出和两组端到端基线

当前能力已经足够支撑阶段 2 的“仿真内核复用”，不需要重写模型本体。

### 2.2 当前架构问题

阶段 2 之前必须正视以下结构问题：

1. `SimulationApp` 同时承担 CLI 参数解析、文件装配、事件应用、循环推进和输出管理，难以复用于 GUI。
2. `ConfigLoader` 与 `CommandSchedule::loadFromCsvFile` 把“文件解析”和“领域对象”耦在一起，不利于引入 XML 与 GUI 配置编辑。
3. 当前配置格式是 JSON，自带 `simple_json` 解析器；需求明确要求改为 XML，并使用 Qt XML。
4. 当前只有“离线批量推进”这一种运行模式，没有“实时时钟驱动”的调度层。
5. 当前绘图脚本只能手动串联，尚未形成统一产物协议。

### 2.3 已确认的外部前提

- Qt 安装路径按需求草稿提供：`/home/sun/Qt5.12.12/5.12.12/gcc_64`
- 工作区中存在 `control.png`，虽然当前尚未纳入版本控制，但已可作为阶段 2 控制面板的直接参考

从图片可提炼出两条关键约束：

1. 舵令按钮应固定为 `左30/左10/左5/舵正/右5/右10/右30`
2. 车令不是单一列表，而是至少分成两个面板：
   - 柴油机车令
   - 燃油机车令

这意味着阶段 2 的控制定义不能再只是“一个 engine_order_map”，而应升级为“带分组信息的车令面板定义”。

## 3. 阶段 2 目标与非目标

### 3.1 功能目标

阶段 2 完成后，系统应提供两条使用路径。

#### 路径 A：GUI 实时仿真

- 启动 Qt 主界面
- 在主界面输入初始纬度、经度、航速、航向
- 点击“开始”后按真实时间推进仿真
- 点击“停止”后结束本次会话并落盘结果
- 界面实时显示经纬度、航向、航速、角速度、当前舵令、当前车令
- 界面实时显示轨迹图
- 舵令/车令通过按钮或组合框下发

#### 路径 B：CLI 批量回放分析

- 使用 XML 配置文件
- 保留 CSV 命令表输入
- 一次命令生成状态 CSV
- 可选自动调用 Python 脚本生成 PNG 图像

### 3.2 非目标

阶段 2 不做以下工作：

- 更高阶操纵模型
- 多船场景
- 网络通信
- 在线地图瓦片
- 复杂数据库或项目文件系统
- 多线程并发仿真

其中“不做多线程”是有意的：当前模型计算量极小，单线程事件循环足以支撑实时性，先避免不必要的同步复杂度。

## 4. 总体设计原则

### 4.1 保留数值内核，重构外围适配层

`SimpleNomotoShipModel` 继续作为阶段 2 的物理内核。重构重点放在：

- 运行时编排
- 配置读取
- UI 展示
- 分析产物输出

### 4.2 让核心逻辑与 Qt Widgets 解耦

Qt 只应进入以下两类模块：

- XML 配置解析与文件流程
- GUI 表现层与进程调用

模型、事件、日志、批量运行逻辑仍保持普通 C++ 类型，避免让 Qt Widgets 侵入数值内核和测试层。

### 4.3 GUI 与 CLI 共用同一套会话引擎

不能做成“两套仿真逻辑”：

- GUI 调用一套实时逻辑
- CLI 再复制一套离线逻辑

正确做法是抽出统一的 `SimulationSession` 或 `SimulationEngine`，由：

- CLI 以“固定步长 while 循环”驱动
- GUI 以“墙钟时间 + 累积器”驱动

### 4.4 状态日志与命令日志分离

当前 `plot_simulation.py` 依赖固定状态 CSV 表头，因此阶段 2 不应贸然破坏：

```text
time_s,lat_deg,lon_deg,heading_deg,speed_mps,yaw_rate_deg_s
```

新增需求通过额外文件补足：

- `state.csv`：状态轨迹，保留现有格式
- `commands.csv`：本次会话命令事件
- `summary.xml` 或 `summary.txt`：本次运行元数据
- `plot.png`：自动绘图产物

这样 GUI 实时会话和 CLI 批量场景都能进入同一分析链路。

## 5. 目标架构

## 5.1 分层结构

建议将当前工程重构为 5 层：

```text
GUI / CLI Frontend
  -> Application Services
      -> Core Simulation Domain
      -> IO Adapters
      -> Analysis Pipeline
```

具体目录建议如下：

```text
src/
├─ core/
│  ├─ common/
│  ├─ model/
│  ├─ scenario/
│  ├─ logger/
│  └─ session/
├─ io/
│  ├─ xml/
│  ├─ csv/
│  └─ artifact/
├─ app/
│  ├─ batch/
│  └─ realtime/
├─ gui/
│  ├─ widgets/
│  ├─ windows/
│  └─ presenters/
├─ cli/
│  └─ main.cpp
└─ gui_main.cpp
```

### 5.2 目标 CMake Targets

建议拆分为以下 targets：

- `ship_motion_core`
- `ship_motion_io`
- `ship_motion_app`
- `ship_motion_gui`
- `ship_motion_cli`
- `ship_motion_tests`

依赖关系：

```text
ship_motion_cli -> ship_motion_app -> ship_motion_core + ship_motion_io
ship_motion_gui -> ship_motion_app -> ship_motion_core + ship_motion_io
ship_motion_tests -> ship_motion_core + ship_motion_io + ship_motion_app
```

Qt 组件建议：

- `Qt5::Core`
- `Qt5::Widgets`
- `Qt5::Xml`
- `Qt5::Test`

不建议阶段 2 引入 `Qt Charts`。轨迹图完全可以用自绘 `QWidget + QPainter` 实现，依赖更少，兼容性更稳。

## 6. 核心模块重构方案

### 6.1 `SimulationSession`：统一会话引擎

新增统一会话引擎，职责是：

- 持有模型实例
- 持有当前仿真时刻
- 应用舵令/车令事件
- 推进一步并返回最新状态
- 记录命令历史

建议接口：

```cpp
class SimulationSession {
public:
    explicit SimulationSession(const SessionConfig& config);

    void applyCommand(const CommandEvent& event);
    ShipState step(double dt_s);

    const ShipState& currentState() const;
    double currentTimeS() const;
    const CommandEvents& commandHistory() const;
};
```

说明：

- `SimpleNomotoShipModel` 继续只关心状态积分
- `SimulationSession` 负责“时间 + 事件 + 日志快照”这一层
- CLI 和 GUI 都只与 `SimulationSession` 打交道

### 6.2 `BatchSimulationRunner`

由当前 `SimulationApp` 演化而来，职责改为：

- 读取 XML 配置
- 读取 CSV 命令
- 构建 `SimulationSession`
- 以固定步长跑到结束
- 输出产物
- 可选调用绘图脚本

当前 `SimulationApp::run()` 中的循环逻辑可整体迁移到这里，但不再直接解析 CLI 或直接依赖 `std::cout`。

### 6.3 `RealtimeSimulationController`

新增 `QObject` 控制器，用于 GUI 实时仿真。

职责：

- 接收“开始/停止/舵令/车令”信号
- 持有 `SimulationSession`
- 用 `QElapsedTimer` 计算真实过去的墙钟时间
- 用 `QTimer` 驱动周期更新
- 通过 signal 发布最新状态、轨迹点和运行状态

推荐机制：

1. GUI 启动后创建 `QTimer`，周期 16ms 或 33ms
2. 每次 `timeout` 读取距离上次 tick 的真实时间
3. 将真实时间累加到 `accumulator_s`
4. 当 `accumulator_s >= dt_s` 时，循环执行 `session.step(dt_s)` 并递减累积器
5. 每次 tick 最多补算有限步数，例如 5 步，防止界面卡死后无限追赶

这样可以保证：

- 仿真时间接近真实时间
- 不因 UI 刷新频率和仿真步长耦死
- 仍然保持数值核心只按固定 `dt_s` 推进

### 6.4 `CommandSchedule` 的职责收缩

当前 `CommandSchedule` 同时做了“容器”和“CSV 解析”。

阶段 2 应拆成：

- `CommandSchedule`：只负责事件排序与 `popReadyEvents`
- `CsvCommandReader`：负责从 CSV 文件读出 `CommandEvents`
- `CommandCsvWriter`：负责把 GUI 会话中的交互事件导出为 CSV

这样 GUI 与 CLI 可以共用同一事件格式，但不会让领域对象知道文件系统。

### 6.5 `ConfigLoader` 替换为 `XmlConfigLoader`

当前 `ConfigLoader` 与 `simple_json` 将被替换为：

- `XmlConfigLoader`
- `XmlConfigWriter`

其中：

- `XmlConfigLoader` 负责把 XML 转成标准 C++ 结构体
- `XmlConfigWriter` 负责配置界面“保存”为 XML

运行时不再保留 JSON 配置解析路径。为降低迁移风险，可提供一次性转换脚本，但运行时统一只认 XML。

## 7. 数据模型设计

### 7.1 保留现有仿真数据结构

以下结构可继续沿用，必要时扩展：

- `SimulationConfig`
- `InitialState`
- `ShipState`
- `CommandEvent`

### 7.2 新增控制定义结构

为适配 GUI 控件和图片中的命令面板，建议新增：

```cpp
struct EngineOrderDefinition {
    std::string id;
    std::string label;
    double target_speed_mps;
    std::string panel_id;
    int display_order {};
};

struct RudderPresetDefinition {
    std::string id;
    std::string label;
    double angle_deg;
    int display_order {};
};
```

建议再补一个车令面板定义：

```cpp
struct EnginePanelDefinition {
    std::string id;
    std::string label;
    int display_order {};
};
```

关键点：

- `id` 用于配置、CSV 和内部逻辑，保持稳定
- `label` 用于 GUI 文案，允许中文
- `panel_id` 用于将车令归属到“柴油机车令”或“燃油机车令”面板
- `display_order` 用于界面按钮排序

这能解决两个现实问题：

1. 图片里的按钮文字是中文，而现有 CLI 和 CSV 中的 `ahead3`、`full` 更适合作为稳定机器标识。
2. 同样存在“全速”“停车”这类标签，但它们属于不同面板；因此内部必须用 `id` 区分，而不能只依赖显示文字。

### 7.3 新增会话配置

建议把“静态配置”和“本次启动参数”分开：

```cpp
struct SessionConfig {
    SimulationConfig simulation;
    InitialState initial_state;
    std::vector<EnginePanelDefinition> engine_panels;
    std::vector<EngineOrderDefinition> engine_orders;
    std::vector<RudderPresetDefinition> rudder_presets;
    std::string default_output_dir;
    bool auto_plot {};
};
```

理由：

- 主界面上的初始经纬度、航向、航速在开始前可编辑
- XML 只提供默认值，不应强制 GUI 每次完全照抄文件

## 8. XML 配置方案

### 8.1 设计原则

XML 应承载静态配置和 GUI 展示配置，不承载“实时命令过程”。

动态命令仍然保持在 CSV 中：

- CLI：直接读取现有命令 CSV
- GUI：运行结束后导出会话事件 CSV

### 8.2 推荐 XML 结构

```xml
<?xml version="1.0" encoding="UTF-8"?>
<shipMotionConfig version="2">
  <simulation dt_s="0.1" duration_s="180.0" earth_radius_m="6378137.0"/>
  <model nomoto_T_s="10.0" nomoto_K="0.08" speed_tau_s="8.0" rudder_limit_deg="35.0"/>
  <initialState lat_deg="24.0" lon_deg="120.0" heading_deg="0.0" speed_mps="0.0"/>
  <analysis default_output_dir="output/scenarios" auto_plot="true" plot_script="scripts/plot_simulation.py"/>
  <enginePanels>
    <panel id="diesel" label="柴油机车令" display_order="0"/>
    <panel id="fuel" label="燃油机车令" display_order="1"/>
  </enginePanels>
  <engineOrders>
    <order id="diesel_ahead_1" panel_id="diesel" label="两进一" target_speed_mps="1.0" display_order="0"/>
    <order id="diesel_ahead_2" panel_id="diesel" label="两进二" target_speed_mps="2.0" display_order="1"/>
    <order id="diesel_ahead_3" panel_id="diesel" label="两进三" target_speed_mps="3.0" display_order="2"/>
    <order id="diesel_ahead_4" panel_id="diesel" label="两进四" target_speed_mps="4.0" display_order="3"/>
    <order id="diesel_ahead_5" panel_id="diesel" label="两进五" target_speed_mps="5.0" display_order="4"/>
    <order id="diesel_ahead_6" panel_id="diesel" label="两进六" target_speed_mps="6.0" display_order="5"/>
    <order id="diesel_full" panel_id="diesel" label="全速" target_speed_mps="6.5" display_order="6"/>
    <order id="diesel_astern_3" panel_id="diesel" label="两退三" target_speed_mps="-1.5" display_order="7"/>
    <order id="diesel_astern_4" panel_id="diesel" label="两退四" target_speed_mps="-2.0" display_order="8"/>
    <order id="diesel_stop" panel_id="diesel" label="停车" target_speed_mps="0.0" display_order="9"/>
    <order id="fuel_ahead_6" panel_id="fuel" label="两进六" target_speed_mps="6.0" display_order="0"/>
    <order id="fuel_ahead_7" panel_id="fuel" label="两进七" target_speed_mps="7.0" display_order="1"/>
    <order id="fuel_ahead_8" panel_id="fuel" label="两进八" target_speed_mps="8.0" display_order="2"/>
    <order id="fuel_ahead_9" panel_id="fuel" label="两进九" target_speed_mps="9.0" display_order="3"/>
    <order id="fuel_ahead_10" panel_id="fuel" label="两进十" target_speed_mps="10.0" display_order="4"/>
    <order id="fuel_full" panel_id="fuel" label="全速" target_speed_mps="10.5" display_order="5"/>
    <order id="fuel_astern_4" panel_id="fuel" label="两退四" target_speed_mps="-2.0" display_order="6"/>
    <order id="fuel_astern_5" panel_id="fuel" label="两退五" target_speed_mps="-2.5" display_order="7"/>
    <order id="fuel_stop" panel_id="fuel" label="停车" target_speed_mps="0.0" display_order="8"/>
  </engineOrders>
  <rudderPresets>
    <preset id="port_30" label="左30" angle_deg="-30" display_order="0"/>
    <preset id="port_10" label="左10" angle_deg="-10" display_order="1"/>
    <preset id="midship" label="舵正" angle_deg="0" display_order="2"/>
    <preset id="starboard_10" label="右10" angle_deg="10" display_order="3"/>
    <preset id="starboard_30" label="右30" angle_deg="30" display_order="4"/>
  </rudderPresets>
</shipMotionConfig>
```

### 8.3 解析实现建议

Qt 5.12 下优先使用 `QDomDocument`，原因是：

- 配置体量小
- 需要配置编辑回写
- DOM 方式更适合表单式修改

如果后续 XML 变大，可再转 `QXmlStreamReader`。阶段 2 先以可维护性为先。

### 8.4 校验规则

`XmlConfigLoader` 启动阶段必须验证：

- `dt_s > 0`
- `duration_s >= 0`
- `earth_radius_m > 0`
- `nomoto_T_s > 0`
- `speed_tau_s > 0`
- `rudder_limit_deg > 0`
- `enginePanels` 非空
- `engineOrders` 非空
- `rudderPresets` 非空
- 任意 `id` 不重复
- 每个 `order.panel_id` 都能映射到已定义面板
- 任意 `display_order` 可解析
- 初始纬度不能过近极点

错误信息要精确到 XML 节点名和属性名，避免当前 JSON 方案那种“字段缺失但上下文不清晰”的问题。

## 9. GUI 方案

### 9.1 主界面布局

主界面建议采用三段式布局：

1. 顶部会话控制区
2. 中部状态与轨迹显示区
3. 底部控制指令区

具体组件：

- 左上：初始经纬度、航向、航速输入框
- 左中：当前状态卡片
- 中央：轨迹图 `TrackViewWidget`
- 右侧：当前舵令/车令、运行时长、开始/停止按钮
- 底部：舵令按钮组 + 柴油机车令面板 + 燃油机车令面板

### 9.2 最低可用控件清单

- `QLineEdit` 或 `QDoubleSpinBox`：初始状态输入
- `QPushButton`：开始、停止、保存配置
- `QLabel`：实时状态显示
- `QListWidget` 或 `QPlainTextEdit`：事件日志
- 自绘 `QWidget`：轨迹图

### 9.3 `TrackViewWidget`

建议用 `QPainter` 自绘，不依赖额外图表库。

显示内容：

- 轨迹折线
- 起点/当前位置
- 当前航向箭头
- 命令触发标记
- 自适应经纬度边界与网格

优点：

- 实现轻量
- 依赖少
- 与当前 Python 图风格一致，便于统一认知

### 9.4 配置编辑策略

不建议阶段 2 单独做复杂“设置窗口”。更高效的做法是：

- 主界面直接展示本次运行的关键输入
- 提供“加载 XML”和“保存 XML”按钮
- 输入控件绑定当前 `SessionConfig`

这样用户既可临时修改一次启动参数，也能持久化为下次默认配置。

## 10. 实时仿真与事件流

### 10.1 GUI 实时数据流

```text
MainWindow
  -> RealtimeSimulationController.start(session_config)
      -> 创建 SimulationSession
      -> 定时 tick
      -> step(dt)
      -> 发出 stateUpdated(state)
  -> MainWindow 刷新状态卡片与轨迹图
  -> 用户点击舵令/车令按钮
      -> controller.applyCommand(event)
      -> session 立即记录并生效
```

### 10.2 开始/停止语义

点击“开始”时：

- 校验输入
- 清空上一会话显示
- 写入初始状态到 `state.csv`
- 切换按钮状态为“运行中”

点击“停止”时：

- 停止定时器
- 导出 `commands.csv`
- 刷新最终状态
- 若 `auto_plot=true`，触发绘图流程

### 10.3 实时显示频率

建议采用两级频率：

- 仿真积分频率：使用 `simulation.dt_s`
- UI 刷新频率：10Hz 到 30Hz

理由：

- 内核积分要按固定步长保证数值语义
- UI 不需要每一步都完全重绘
- 轨迹点可以每步记录，但界面可按较低频率刷新

### 10.4 自动停止条件

GUI 应同时支持两种停止方式：

- 用户点击“停止”
- 仿真时间达到 `duration_s` 自动停止

如果后续需要“无限运行直到手动停止”，可约定 `duration_s = 0` 表示无上限。

## 11. CLI 与自动绘图方案

### 11.1 CLI 兼容原则

阶段 2 的 CLI 仍保留当前使用习惯，只修改配置格式：

```bash
./build/ship_motion_sim_cli \
  --config config/default_config.xml \
  --commands config/sample_commands.csv \
  --output output/run/state.csv
```

新增可选参数：

- `--plot-output <path>`
- `--artifacts-dir <dir>`
- `--no-plot`

### 11.2 产物目录规范

推荐每次运行输出到一个专属目录：

```text
output/runs/<timestamp>/
  ├─ state.csv
  ├─ commands.csv
  ├─ plot.png
  └─ summary.txt
```

CLI 如果只传 `--output`，则兼容当前行为；如果传 `--artifacts-dir`，则自动生成完整产物集合。

### 11.3 自动绘图实现

新增 `AnalysisPipeline` 或 `PlotRunner`，使用 `QProcess` 调用：

```text
python3 scripts/plot_simulation.py --input state.csv --commands commands.csv --output plot.png
```

使用 `QProcess` 而不是 `std::system` 的原因：

- 可以拿到退出码和错误输出
- GUI 和 CLI 可以共用
- 与 Qt 工程整体风格一致

### 11.4 GUI 会话复用同一绘图链路

GUI 停止后，将本次交互事件导出为 `commands.csv`，再走同一条 `plot_simulation.py` 流程。

这意味着：

- Python 脚本只维护一份
- GUI 与 CLI 的结果图风格天然一致
- 回归验证更简单

## 12. CMake 与构建调整

### 12.1 Qt 查找

按需求草稿加入：

```cmake
list(APPEND CMAKE_PREFIX_PATH "/home/sun/Qt5.12.12/5.12.12/gcc_64")
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Xml Test)
```

### 12.2 构建产物

建议生成：

- `build/ship_motion_sim_cli`
- `build/ship_motion_sim_gui`
- `build/ship_motion_sim_tests`

### 12.3 清理旧依赖

当 XML 迁移完成且测试通过后，可删除：

- `src/common/simple_json.h`
- `src/common/simple_json.cpp`

这是阶段 2 的明确收口动作，避免双格式长期并存。

## 13. 测试与验证方案

### 13.1 核心回归测试

保留并迁移现有测试能力：

- 速度响应
- 航向响应
- 经纬度换算
- 命令排序
- 状态 CSV 输出
- 端到端基线

但要把 JSON fixture 全部迁成 XML fixture。

### 13.2 新增测试维度

#### XML 配置测试

- 合法 XML 正常加载
- 缺失节点报错
- 缺失属性报错
- 重复 `id` 报错
- 极点附近纬度报错

#### 批量运行测试

- CLI 从 XML + CSV 产出状态 CSV
- `--artifacts-dir` 自动生成 `commands.csv`
- 自动绘图开关行为正确

#### 实时控制测试

重点不是测 `QTimer` 本身，而是测控制器的可测试核心方法，例如：

```cpp
controller.advanceByElapsed(0.25);
```

验证：

- 真实时间被拆成若干 `dt` 步
- 当前状态正确推进
- 命令事件即时生效

#### GUI 烟雾测试

使用 `QtTest` 覆盖最小交互路径：

- 输入初始状态
- 点击开始
- 点击一个舵令按钮
- 点击一个车令按钮
- 点击停止
- 检查状态标签变化与产物文件落盘

### 13.3 验收标准

阶段 2 完成的验收线应为：

1. GUI 可以启动并手动开始/停止仿真。
2. GUI 中修改初始经纬度、航向、航速后会正确生效。
3. GUI 中点击舵令/车令后，状态显示与轨迹会持续更新。
4. CLI 可以从 XML 配置和 CSV 命令运行批量仿真。
5. CLI 或 GUI 停止后都能产出统一格式的分析文件。
6. 自动绘图链路可用，失败时能返回明确错误。
7. 现有数值回归结果在 XML 迁移后保持一致。

## 14. 实施顺序

建议按以下顺序落地，避免同时改太多层。

### 第 1 步：提炼核心会话层

- 引入 `SimulationSession`
- 让 CLI 改为通过会话层运行
- 保持现有 JSON 配置暂时可用，只做结构重构

### 第 2 步：引入 XML 配置

- 实现 `XmlConfigLoader/Writer`
- 迁移 `config/` 和 `tests/fixtures/`
- 删除运行时 JSON 依赖

### 第 3 步：补齐批量产物链路

- 引入 `CommandCsvWriter`
- 引入 `PlotRunner`
- 支持 `--artifacts-dir` 与自动绘图

### 第 4 步：实现实时控制器

- 完成 `RealtimeSimulationController`
- 编写控制器测试

### 第 5 步：实现 Qt 主界面

- 先做最小布局
- 再接入轨迹图和事件面板
- 最后做 XML 加载/保存按钮

### 第 6 步：收尾与清理

- 删除 `simple_json`
- 清理旧 `SimulationApp`
- 更新 README、架构文档和示例配置

## 15. 风险与待确认项

### 15.1 `control.png` 缺失

这是当前唯一会直接影响 GUI 细节的外部缺口。

当前可先做：

- 可配置的舵令预设列表
- 可配置的车令预设列表
- 通用按钮面板

仍待确认的只剩两类信息：

- 图片中的车令档位与目标速度映射关系
- GUI 是否需要完全复刻图片视觉样式，还是只复刻命令结构

### 15.2 现有文档与样例存在轻微漂移

例如 `README.md` 中“三分钟示例”与当前 `three_minute_config.json` 的 `duration_s=600.0` 并不一致。阶段 2 应顺手清理这类文档漂移，否则 GUI/CLI 示例会继续误导。

### 15.3 Qt 引入后的测试环境变化

原项目几乎不依赖外部库。引入 Qt 后：

- 构建机必须能找到 Qt
- GUI 测试在无显示环境下可能需要 `offscreen` 平台

因此 CTest 中应为 GUI 测试预留无头运行策略。

## 16. 结论

阶段 2 的正确实现路径不是“在当前 CLI 上硬塞一个 Qt 窗口”，而是：

1. 保留 `SimpleNomotoShipModel` 作为稳定数值内核。
2. 抽出统一的 `SimulationSession`，让 GUI 与 CLI 共用。
3. 用 Qt XML 替换 JSON 配置，并把控制预设也纳入 XML。
4. GUI 采用单线程实时控制器和自绘轨迹图，避免过早引入复杂并发和重图表依赖。
5. 批量 CLI 与 GUI 会话统一落到同一组分析产物，继续复用现有 Python 绘图脚本。

按这一路线实施，阶段 2 可以在不推翻当前模型和测试资产的前提下，平滑演进为“GUI 实时控制 + CLI 批量分析”双模系统。

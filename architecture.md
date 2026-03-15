# Ship Motion Sim Architecture

## 1. 当前定位

当前仓库已经收敛为 simplify 版本，只保留一个 Qt 实时控制入口和最小仿真内核，目标是作为后续宿主系统集成的轻量组件。

项目不再承担以下能力：

- CLI 批量回放
- CSV 场景调度
- 自动绘图
- 轨迹图展示
- 多产物目录管理

## 2. 可执行入口

当前 CMake 生成两个二进制：

- `ship_motion_sim_gui`
- `ship_motion_sim_tests`

其中：

- `ship_motion_sim_gui` 是唯一运行入口
- `ship_motion_sim_tests` 是最小测试入口

## 3. 模块分层

### 3.1 入口层

文件：

- `src/gui_main.cpp`

职责：

- 创建 Qt 应用
- 解析可选的 `--config`
- 构造主窗口
- 捕获顶层异常

### 3.2 应用层

文件：

- `src/app/realtime_simulation_controller.*`
- `src/app/simulation_session.*`
- `src/config/xml_config_loader.*`

职责：

- 读取 XML 配置
- 在首次命令点击时惰性启动仿真
- 管理单次仿真会话
- 将状态写入 `../Logs/` 下按时间戳命名的 CSV

### 3.3 领域模型层

文件：

- `src/model/simple_nomoto_ship_model.*`

职责：

- 保存船舶内部状态
- 响应当前舵令和车令
- 推进一步并输出状态快照

### 3.4 基础设施层

文件：

- `src/logger/state_logger.*`
- `src/common/*`

职责：

- 写出状态 CSV
- 保存公共数据结构
- 提供角度与经纬度换算工具

### 3.5 表现层

文件：

- `src/gui/main_window.*`

职责：

- 呈现 3 个控制组
- 将按钮事件转发给实时控制器
- 维持当前舵令和车令的选中态反馈

## 4. 核心对象关系

当前依赖关系如下：

```text
GUI Main
  -> parse --config or use ../config/default_config.xml
  -> MainWindow
      -> XmlConfigLoader
      -> RealtimeSimulationController
          -> SessionConfig
          -> SimulationSession
              -> SimpleNomotoShipModel
          -> StateLogger
```

依赖方向保持简单：

- GUI 不直接操作模型
- 模型不依赖 Qt Widgets
- 日志格式集中在 `StateLogger`

## 5. 核心数据模型

当前仍保留的关键公共结构有：

- `SimulationConfig`
- `InitialState`
- `ShipState`
- `CommandType`
- `CommandEvent`
- `EnginePanelDefinition`
- `EngineOrderDefinition`
- `RudderPresetDefinition`
- `SessionConfig`

这些结构足以支撑 simplify 分支的实时交互需求。

## 6. 运行时数据流

当前 GUI 数据流为：

```text
MainWindow
  -> 用户点击任一舵令/车令按钮
  -> RealtimeSimulationController.ensureSessionStarted()
  -> 创建 ../Logs/<timestamp>.csv
  -> StateLogger 写表头和初始状态
  -> QTimer + QElapsedTimer
  -> SimulationSession.step(dt)
  -> StateLogger 持续写状态行
  -> MainWindow 同步按钮选中态
  -> 用户关闭窗口
  -> controller.stop()
  -> 关闭 CSV 文件
```

关键语义：

- 启动是惰性的，由第一条命令触发
- 停止由窗口关闭触发
- 会话只保留当前状态，不保留全量历史

## 7. 会话设计

`SimulationSession` 现在只承担单次会话的最小职责：

- 持有 `SimpleNomotoShipModel`
- 维护当前仿真时间
- 应用舵令/车令
- 暴露当前状态、当前舵令、当前车令

它不再负责：

- 全量状态历史
- 全量命令历史
- 产物导出相关信息

这样可以避免 simplify 分支在无限运行场景下持续增长内存。

## 8. 日志策略

状态日志统一由 `StateLogger` 写到：

```text
../Logs/YYYYMMDD_HHMMSS_zzz.csv
```

CSV 表头固定为：

```text
time_s,lat_deg,lon_deg,heading_deg,speed_mps,yaw_rate_deg_s
```

`StateLogger` 在写表头和每条状态后立即 flush，便于外部系统实时读取文件。

## 9. 当前边界

当前 simplify 版本的明确边界是：

- 不支持离线命令脚本回放
- 不支持自动绘图
- 不支持轨迹图显示
- 默认配置来自 `../config/default_config.xml`，也可通过 `--config` 指向外部 XML

这保证了当前分支的复杂度集中在最小实时控制路径上。

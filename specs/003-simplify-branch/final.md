# 简化分支技术方案

## 1. 目标

基于当前 `main` 分支形成一个长期维护的 `simplify` 分支，用于后续集成到其他系统。该分支只保留核心实时控制能力，移除与独立演示工程相关的输入、产物和分析链路。

本方案的目标不是再做一套新架构，而是在尽量复用现有模型层的前提下，把复杂度集中从 GUI、配置读取和产物输出链路上剥离掉，降低后续分支同步成本。

## 2. 需求归纳

结合 `draft.md`，简化分支应满足以下约束：

1. `main` 继续保留完整工程能力和完整测试。
2. `simplify` 只保留主界面中的 3 个控制组：
   - 舵令
   - 柴油机车令
   - 燃油机车令
3. 不再从界面输入初始经纬度、航向、航速。
4. 默认配置从 XML 读取，默认路径为 `../config/default_config.xml`；若读取失败则直接报错，不自行生成默认配置。
5. 移除“开始”“停止”按钮。
6. 点击任一舵令或车令按钮即视为启动仿真。
7. 关闭窗口时停止仿真。
8. 不再向 `output/` 写任何文件。
9. 状态日志继续输出为 CSV 文件，按时间命名并固定写入 `../Logs/` 目录。

## 3. 当前实现与差距

### 3.1 当前 GUI

当前 `src/gui/main_window.*` 同时承担以下职责：

- XML 配置加载与保存
- 初始状态输入
- 开始/停止控制
- 实时状态展示
- 输出目录展示
- 事件日志展示
- 轨迹图展示
- 舵令与两组车令面板构建

这与“只保留 3 个 group”的目标差距很大。

### 3.2 当前运行时控制

当前 `src/app/realtime_simulation_controller.*` 的语义是：

- `start(config)` 显式创建会话并启动 `QTimer`
- `stop()` 显式停止并导出 `state.csv`、`commands.csv`、`summary.txt`
- 可选调用 `PlotRunner` 生成 `plot.png`

这与“首次命令触发启动、关闭窗口停止、不写文件”的目标不一致。

### 3.3 当前配置与依赖

当前 GUI 强依赖：

- `XmlConfigLoader`
- `XmlConfigWriter`
- `default_config.xml`
- `analysis.default_output_dir`
- 自动绘图脚本路径

这些都是独立演示工程的便利能力，不是简化分支的核心能力。

### 3.4 当前 `SimulationSession` 的真实职责

当前 `SimulationSession` 并不是“管理多次模拟”的对象。它实际只负责单次会话内的：

- 模型持有
- 当前时刻推进
- 舵令/车令应用
- 当前状态维护
- 历史记录维护

因此，问题不在于“它是否用于多次模拟”，而在于“简化分支是否还需要它记录完整历史”。

## 4. 总体设计原则

### 4.1 保留内核，裁剪外层

保留以下稳定内核：

- `SimpleNomotoShipModel`
- `SimulationSession`
- `StateLogger`
- 舵令/车令的数据定义

优先裁剪以下外层能力：

- CLI 批量回放
- CSV 命令读写
- 产物目录管理
- 自动绘图
- GUI 上的大量状态展示部件

### 4.2 减少与 `main` 的长期分叉

简化分支不建议重命名核心类，也不建议重新设计模型层接口。这样后续从 `main` 同步模型修复时冲突最小。

### 4.3 文件日志优先于标准输出

状态日志不应走 `stdout`，因为 GUI 进程的标准输出很容易与诊断信息、宿主拉起方式或运行环境耦合，作为集成协议不稳定。

简化分支应继续沿用 CSV 文件作为状态输出载体，并约束为：

- 固定输出目录：`../Logs/`
- 文件名按时间命名
- 仍由 `StateLogger` 负责写出统一列格式

普通诊断信息仍建议走：

- `stderr`
- Qt 警告框

## 5. 推荐改造方案

### 5.1 保留 XML 配置加载作为 simplify 分支入口

简化分支应继续保留 `XmlConfigLoader`，不应改成代码内置默认 `SessionConfig`。

推荐行为：

- GUI 默认读取相对于可执行文件目录的 `../config/default_config.xml`
- 若后续需要覆盖，可继续保留 `--config <path>` 参数
- 若配置文件不存在或解析失败，程序直接报错退出

配置文件仍承担以下内容：

- 仿真参数
- 初始状态
- 舵令预设
- 柴油机车令定义
- 燃油机车令定义

为满足 simplify 分支“关闭窗口时停止”的语义，推荐把默认配置文件中的 `simulation.duration_s` 设为 `0.0`。

这样做的收益是：

- 保留与 `main` 分支一致的核心配置机制
- 降低未来同步配置相关修复时的冲突
- 宿主系统仍可通过替换 XML 文件完成参数接管

### 5.2 GUI 仅保留三个控制组

直接收缩 `src/gui/main_window.*`，只保留：

- 舵令组
- 柴油机车令组
- 燃油机车令组

应删除以下界面元素：

- 加载 XML 按钮
- 保存 XML 按钮
- 开始按钮
- 停止按钮
- 初始状态输入区
- 实时状态区
- 输出目录显示
- 事件日志区
- 轨迹图

界面仍需保留按钮的持续选中态反馈。原因不是视觉细节，而是当前 lessons 已明确指出：Qt 控制按钮必须持续显示当前生效命令，否则用户无法判断正在执行的舵令和车令。

因此：

- 舵令按钮继续使用互斥 `QButtonGroup`
- 每组车令按钮继续保持可选中态
- 当前选中态由控制器的当前命令同步，而不是只在 click 时闪一下

### 5.3 启停语义改为惰性启动

`RealtimeSimulationController` 应从“显式 start/stop 控制器”改为“命令驱动的惰性启动控制器”。

推荐行为如下：

1. 窗口打开时，不自动启动仿真。
2. 第一次点击任一舵令或车令按钮时：
   - 若还没有 session，则创建 session
   - 创建 `../Logs/` 目录
   - 按当前时间生成本次会话的 CSV 路径
   - 初始化 `StateLogger(file_stream)`
   - 输出表头
   - 输出初始状态
   - 启动定时器
   - 立刻应用本次点击对应的命令
3. 后续按钮点击只更新当前命令，不重复初始化会话。
4. 窗口关闭时，如果 session 存在，则停止定时器并结束本次会话。

建议新增内部方法：

- `ensureSessionStarted()`

并在以下入口内部调用：

- `applyRudderCommand(...)`
- `applyEngineCommand(...)`

这样 `MainWindow` 不再需要显式 `handleStart()`。

### 5.4 停止时不再写任何产物

`RealtimeSimulationController::stop()` 在简化分支中应只做：

- 停止 `QTimer`
- 清理运行态标记
- flush 并关闭当前状态 CSV

应删除或停用以下逻辑：

- `buildArtifactPaths()`
- `writeSessionArtifacts(...)`
- `CsvCommandWriter::writeToFile(...)`
- `PlotRunner::run(...)`
- `summary.txt` 写出
- `sessionFinished(artifacts_dir)` 这类基于输出目录的信号

如果保留 `runningChanged(bool)`，仍然有价值；但 `lastArtifacts()` 和 `ArtifactPaths` 在 GUI 路径中将不再需要。

### 5.5 `StateLogger` 改为实时 CSV 文件输出

当前 `StateLogger` 已支持任意 `std::ostream`。简化分支应继续把它接到文件流，但统一约束输出目录与命名规则：

- 目录固定为 `../Logs/`
- 文件名建议使用时间戳，例如 `YYYYMMDD_HHMMSS_zzz.csv`

- 会话启动时输出表头和初始状态
- 每次积分推进后输出最新状态

为保证外部系统按文件实时读取，建议在 `writeHeader()` 和 `writeState()` 后显式 `flush()`，避免缓冲导致观测延迟。

建议保持 CSV 内容格式不变，继续沿用当前表头：

```text
time_s,lat_deg,lon_deg,heading_deg,speed_mps,yaw_rate_deg_s
```

这样外部系统复用现有解析逻辑即可。

### 5.6 `SimulationSession` 建议保留，但要收缩职责

结论：`SimulationSession` 不应删除。

理由如下：

1. 它本质上是单次仿真的领域对象，不是“多次模拟管理器”。
2. 它隔离了 GUI 与模型层，当前依赖方向是健康的。
3. 删除它会把命令应用、时钟推进、模型状态维护重新散落到控制器里，反而让简化分支更难维护。

但建议收缩它的职责：

- 保留：当前状态、当前时间、当前舵令、当前车令、`step()`、`applyCommand()`
- 删除或停用：完整 `state_history_`、`command_history_`

原因是简化分支移除了轨迹图和文件导出后，持续积累全量历史只会带来无上限内存增长，而没有实际收益。

如果为了第一版尽快落地，短期也可以先保留历史字段，但这只是过渡方案，不是推荐终态。

### 5.7 简化分支建议裁掉的文件与依赖

推荐从简化分支构建图中移除以下能力：

- `src/app/batch_simulation_runner.*`
- `src/app/plot_runner.*`
- `src/app/simulation_app.*`
- `src/scenario/command_schedule.*`
- `src/scenario/csv_command_io.*`
- `src/gui/track_view_widget.*`
- CLI 入口 `src/main.cpp`

对应地，`CMakeLists.txt` 可收缩为仅构建：

- 核心库
- GUI 库
- GUI 可执行文件
- 简化分支自己的最小测试

同时可移除以下外部要求：

- Python 绘图依赖
- `matplotlib`

### 5.8 分支同步策略

为了让 `simplify` 分支能长期跟进 `main`，建议采用以下边界：

- `main` 继续承载完整功能、完整测试、文档和演示链路
- `simplify` 只在 GUI 层和应用层分叉
- 模型层、数学层、日志格式尽量与 `main` 保持一致

这意味着后续从 `main` 向 `simplify` 同步时，主要冲突会集中在：

- `src/gui/main_window.*`
- `src/app/realtime_simulation_controller.*`
- `CMakeLists.txt`

这是可控的。

## 6. 实施步骤

### 阶段 1：建立最小可运行 simplify 分支

1. 从 `main` 创建 `simplify`。
2. 保留 XML 配置读取，默认指向 `../config/default_config.xml`。
3. 精简 `MainWindow` 到 3 个 group。
4. 把控制器改为首次命令触发启动。
5. 停止时移除全部产物写出逻辑。
6. 将 `StateLogger` 绑定到 `../Logs/` 下按时间命名的 CSV 文件，并补充 flush。

### 阶段 2：删除无用路径

1. 从构建脚本中移除 CLI、plot、scenario 相关源文件，但保留 XML 配置读取。
2. 删除轨迹图及其依赖。
3. 删除基于 artifacts 的结构和信号。
4. 删除 `SimulationSession` 的历史记录能力。

### 阶段 3：补齐分支级验证

1. 更新 README，明确 simplify 分支的边界和用法。
2. 更新 architecture 文档，说明新数据流。
3. 收敛测试到简化分支的核心路径。

## 7. 测试与验证建议

简化分支虽然不再承担 `main` 的完整测试集，但仍应保留足够证明核心行为正确的验证。

建议至少保留以下测试：

1. 模型推进测试：
   - 速度响应
   - 航向响应
   - 经纬度换算
2. `SimulationSession` 测试：
   - 命令生效
   - 时间推进
   - 当前舵令/车令状态
3. `RealtimeSimulationController` 测试：
   - 初始时未启动
   - 首次点击命令后自动启动
   - `../Logs/` 下生成时间命名的 CSV
   - CSV 有表头和状态行
   - 关闭窗口或显式 stop 后停止推进
4. GUI 烟雾测试：
   - 仅存在 3 个控制组
   - 不存在开始/停止按钮
   - 按钮点击后保持当前选中态

特别注意：既然 CSV 文件是集成协议的一部分，测试应直接校验文件路径规则与输出格式，而不是只校验函数被调用。

## 8. 风险与处理

### 8.1 风险：无限运行导致内存增长

如果保持当前 `SimulationSession` 的全量历史记录，同时取消自动停止，那么长期运行会持续占用内存。

处理建议：

- 在 simplify 分支删除历史记录能力

### 8.2 风险：状态日志与运行环境耦合

如果继续依赖 `stdout` 承载状态日志，实际集成时很容易受到宿主拉起方式、缓冲行为和普通诊断输出的影响。

处理建议：

- 状态日志统一写入 `../Logs/` 下按时间命名的 CSV
- 诊断信息改走 `stderr` 或 GUI 提示

### 8.3 风险：分支长期漂移

如果在 simplify 分支里重命名大量类或重写模型层，后续与 `main` 的同步成本会迅速升高。

处理建议：

- 保留 `SimulationSession`
- 保留核心数据结构命名
- 把分歧限制在 GUI 和应用层

## 9. 最终结论

推荐的简化分支方案是：

- 保留模型层和 `SimulationSession`
- 保留 XML 配置读取，默认使用 `../config/default_config.xml`
- GUI 只保留舵令与两组车令
- 首次命令点击即启动
- 关闭窗口即停止
- 状态继续通过 `StateLogger` 写入 `../Logs/` 下按时间命名的 CSV
- 删除产物目录、自动绘图、CLI、轨迹图和相关依赖

对草稿中的问题，明确结论如下：

`SimulationSession` 不建议删除。应保留它作为“单次仿真会话”的核心对象，但要在 simplify 分支中删除历史记录能力，并继续按既定建议控制分支漂移范围。

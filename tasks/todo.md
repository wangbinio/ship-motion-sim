# 本次任务计划

- [x] 阅读 `specs/003-simplify-branch/final.md`，确认本轮要把方案落为实际代码，而不是继续停留在文档层
- [x] 审查当前 GUI、实时控制器、`SimulationSession`、构建脚本和测试，识别与 simplify 目标冲突的代码路径
- [x] 保留 XML 配置读取入口，并将 GUI 精简为舵令、柴油机车令、燃油机车令 3 个控制组
- [x] 改造 `RealtimeSimulationController` 为首次命令触发启动、窗口关闭停止、固定写入 `../Logs/` 时间命名 CSV
- [x] 删除 `SimulationSession` 历史记录能力，并清理轨迹图、产物导出、CLI 等不再使用的构建依赖
- [x] 更新测试与必要文档，执行构建和测试验证

# Review

- 已保留 `src/config/xml_config_loader.*` 作为 simplify 分支的配置入口，GUI 默认读取相对于可执行文件目录的 `../config/default_config.xml`，读取失败直接报错退出。
- 已重写 `src/gui/main_window.*`，界面只保留舵令、柴油机车令、燃油机车令 3 个控制组，移除了开始/停止、配置读写、状态区、事件日志和轨迹图。
- 已重写 `src/app/realtime_simulation_controller.*`，改为首次命令惰性启动，关闭窗口停止，并固定将状态 CSV 写到 `../Logs/` 下按时间戳命名的文件。
- 已修改 `src/logger/state_logger.cpp` 以在写表头和状态行后立即 flush，便于外部系统实时读取日志文件。
- 已收缩 `src/app/simulation_session.*`，删除全量状态历史和命令历史，只保留当前状态、当前时刻和当前命令语义。
- 已将 `config/default_config.xml` 的 `duration_s` 调整为 `0.0`，避免默认配置与“关闭窗口时停止”的 simplify 语义冲突。
- 已从仓库中删除 simplify 分支不再保留的源码入口：CLI、批量回放、CSV 场景、绘图与轨迹图相关文件。
- 已更新 `CMakeLists.txt`、`README.md`、`architecture.md` 与 `specs/003-simplify-branch/final.md`，当前文档与实现都已切换为“保留 XML 配置读取”的 simplify 版本。
- 已重写 `tests/test_main.cpp`，覆盖模型响应、默认 XML 配置读取、会话命令跟踪、控制器惰性启动与日志输出、GUI 三组按钮及选中态反馈。
- 已执行 `cmake -S . -B build`、`cmake --build build -j4` 与 `ctest --test-dir build --output-on-failure`，结果通过。

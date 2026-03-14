# Ship Motion Sim

一个基于一阶 Nomoto 航向模型和一阶速度响应模型的单舰二维平面机动仿真器，当前同时支持：

- Qt 实时交互界面
- XML 配置加载与保存
- CLI 批量回放
- CSV 日志输出
- Python 脚本自动绘图

项目目标仍然是一个简单、稳定、可测试的工程原型，而不是高保真操纵仿真平台。

## 当前能力

- 从 XML 配置加载模型参数、初始状态、舵令预设和车令面板
- GUI 中实时输入初始经纬度、航向和航速
- GUI 中通过舵令按钮和两组车令面板控制仿真
- CLI 中从 CSV 命令表批量回放场景
- 输出状态 `time_s, lat_deg, lon_deg, heading_deg, speed_mps, yaw_rate_deg_s`
- 导出 `state.csv`、`commands.csv`、`summary.txt`
- 可选自动调用 `scripts/plot_simulation.py` 生成 `plot.png`

## 模型

航向模型：

```text
T * r_dot + r = K * delta
psi_dot = r
```

速度模型：

```text
tau_u * u_dot + u = u_cmd
```

位置模型：

```text
x_dot = u * cos(psi)
y_dot = u * sin(psi)
```

经纬度换算仍然采用局部切平面近似，适用于局部小范围轨迹。

## 构建

要求：

- CMake >= 3.16
- 支持 C++17 的编译器
- Qt 5.12+
- Python 3
- 若需要自动绘图，还需要 `matplotlib`

当前工程默认按以下路径查找 Qt：

```cmake
/home/sun/Qt5.12.12/5.12.12/gcc_64
```

构建命令：

```bash
cmake -S . -B build
cmake --build build
```

生成的可执行文件：

- `build/ship_motion_sim`
- `build/ship_motion_sim_gui`
- `build/ship_motion_sim_tests`

## 配置文件

默认配置：

- `config/default_config.xml`
- `config/three_minute_config.xml`

XML 结构包含：

- `simulation`
- `model`
- `initialState`
- `analysis`
- `enginePanels`
- `engineOrders`
- `rudderPresets`

默认控制面板与 `control.png` 一致：

- 舵令：`左30/左10/左5/舵正/右5/右10/右30`
- 车令：分为“柴油机车令”和“燃油机车令”两组

## CLI 用法

最小运行：

```bash
./build/ship_motion_sim \
  --config config/default_config.xml \
  --commands config/sample_commands.csv \
  --output output/manual/state.csv \
  --no-plot
```

生成完整产物目录：

```bash
./build/ship_motion_sim \
  --config config/default_config.xml \
  --commands config/sample_commands.csv \
  --artifacts-dir output/runs/manual_cli
```

支持参数：

- `--config <path>`：XML 配置路径
- `--commands <path>`：命令 CSV 路径
- `--output <path>`：状态 CSV 输出路径
- `--artifacts-dir <dir>`：统一产物目录
- `--plot-output <path>`：自定义图片输出路径
- `--no-plot`：禁用自动绘图

CLI 的命令 CSV 格式：

```text
time_s,type,value
0.0,engine,diesel_ahead_3
5.0,rudder,10
15.0,rudder,0
20.0,engine,diesel_full
```

说明：

- `type=engine` 时，`value` 是 XML 中定义的车令 `id`
- `type=rudder` 时，`value` 是舵角，单位为度

## GUI 用法

启动 GUI：

```bash
./build/ship_motion_sim_gui --config config/default_config.xml
```

使用流程：

1. 加载 XML 配置
2. 修改初始经纬度、航向、航速
3. 点击“开始”
4. 使用舵令和车令按钮实时控制
5. 点击“停止”后自动导出本次会话产物

GUI 默认把每次会话输出到 XML 中 `analysis.default_output_dir` 指定的目录下，并按时间戳创建子目录。

## 产物结构

CLI 使用 `--artifacts-dir`，或者 GUI 停止后，都会产出：

```text
<artifacts-dir>/
  ├─ state.csv
  ├─ commands.csv
  ├─ summary.txt
  └─ plot.png
```

若关闭自动绘图，则不会生成 `plot.png`。

## 绘图

绘图脚本仍然使用：

```bash
python3 scripts/plot_simulation.py \
  --input output/runs/manual_cli/state.csv \
  --commands output/runs/manual_cli/commands.csv \
  --output output/runs/manual_cli/plot.png
```

脚本会生成包含以下信息的总览图：

- 轨迹图
- 航向曲线
- 航速曲线
- 转首角速度曲线
- 命令时间线

## 测试

执行：

```bash
ctest --test-dir build --output-on-failure
```

当前测试覆盖：

- 模型速度/航向响应
- 经纬度换算
- CSV 命令读取与调度顺序
- XML 配置读写
- 会话历史记录
- 批量运行器产物输出
- CLI 输出
- 实时控制器推进
- GUI 构造烟雾测试
- 两组端到端基线场景

## 已知边界

- 仍然只支持单舰
- 不包含 3-DOF 或 MMG 模型
- 不包含风浪流扰动
- 不包含舵机速率限制
- GUI 轨迹图是自绘简图，不是在线地图
- `control.png` 已确定控制结构，但不同车令的目标速度映射仍然属于工程约定值，不是实船标定值

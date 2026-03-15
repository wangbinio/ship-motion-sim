# Ship Motion Sim

当前仓库已经按 `specs/003-simplify-branch/final.md` 收敛为简化集成版本，只保留 Qt 实时控制界面和核心船舶运动模型。

## 当前能力

- 主界面只保留 3 个控制组：
  - 舵令
  - 柴油机车令
  - 燃油机车令
- 启动时从 XML 读取仿真参数、初始状态和控制面板定义
- 点击任一舵令或车令按钮即启动仿真
- 关闭窗口时停止仿真
- 状态持续输出到 `../Logs/` 下按时间戳命名的 CSV 文件
- 保留一阶 Nomoto 航向模型和一阶速度响应模型

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

- `build/ship_motion_sim_gui`
- `build/ship_motion_sim_tests`

## GUI 用法

启动 GUI：

```bash
./build/ship_motion_sim_gui
```

默认会读取相对于可执行文件目录的 `../config/default_config.xml`。
如果文件不存在或解析失败，程序会直接报错退出。

也可以显式指定配置文件：

```bash
./build/ship_motion_sim_gui --config /abs/path/to/config.xml
```

使用流程：

1. 打开程序
2. 点击任一舵令或车令按钮启动仿真
3. 继续通过 3 个控制组切换当前舵令和车令
4. 关闭窗口结束本次会话

## 日志输出

每次会话都会在 `../Logs/` 下生成一个按时间戳命名的 CSV，例如：

```text
../Logs/20260315_153000_123.csv
```

CSV 表头固定为：

```text
time_s,lat_deg,lon_deg,heading_deg,speed_mps,yaw_rate_deg_s
```

## 测试

执行：

```bash
ctest --test-dir build --output-on-failure
```

当前测试覆盖：

- 模型速度/航向响应
- 经纬度换算
- 默认 XML 配置读取
- `SimulationSession` 当前命令与推进语义
- `RealtimeSimulationController` 的惰性启动与日志输出
- GUI 三组控制面板与按钮选中态反馈

## 已知边界

- 仍然只支持单舰
- 不包含 3-DOF 或 MMG 模型
- 不包含风浪流扰动
- 不包含舵机速率限制
- 默认配置文件仍由 simplify 分支随仓库维护，后续宿主系统可替换 `default_config.xml` 或通过 `--config` 指定外部配置
- `control.png` 已确定控制结构，但不同车令的目标速度映射仍然属于工程约定值，不是实船标定值

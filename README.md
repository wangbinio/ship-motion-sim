# Ship Motion Sim

一个最小可运行的舰艇二维平面机动仿真器，基于：

- 一阶 Nomoto 航向模型
- 一阶速度响应模型
- 局部切平面位置积分
- 经纬度日志输出

项目目标不是高保真船舶操纵仿真，而是提供一个简单、稳定、可测试的 CLI 原型。

## 功能

- 从 JSON 配置加载初始状态和模型参数
- 从 CSV 命令表加载舵令与车令
- 以固定时间步推进状态
- 输出 `time_s, lat_deg, lon_deg, heading_deg, speed_mps, yaw_rate_deg_s`
- 支持通过 `--output` 将结果写入文件

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

经纬度换算采用局部切平面近似，适用于局部小范围航迹。

## 目录

```text
.
├─ CMakeLists.txt
├─ config/
│  ├─ default_config.json
│  └─ sample_commands.csv
├─ src/
├─ tests/
├─ draft.md
├─ final.md
└─ README.md
```

## 构建

要求：

- CMake >= 3.16
- 支持 C++17 的编译器

构建命令：

```bash
cmake -S . -B build
cmake --build build
```

生成的可执行文件：

- `build/ship_motion_sim`
- `build/ship_motion_sim_tests`

## 运行

标准输出运行：

```bash
./build/ship_motion_sim \
  --config config/default_config.json \
  --commands config/sample_commands.csv
```

写入文件运行：

```bash
./build/ship_motion_sim \
  --config config/default_config.json \
  --commands config/sample_commands.csv \
  --output output/sim_run.csv
```

说明：

- 如果 `--output` 的父目录不存在，程序会自动创建。
- 如果不传 `--output`，CSV 会写到标准输出。

## 3 分钟示例场景

仓库内提供了一组 180 秒场景文件：

- `config/three_minute_config.json`
- `config/three_minute_commands.csv`

命令表按 20 秒间隔安排事件，混合了三类控制：

- 纯车令
- 纯舵令
- 同一时刻同时下发车令和舵令的组合命令

生成日志：

```bash
./build/ship_motion_sim \
  --config config/three_minute_config.json \
  --commands config/three_minute_commands.csv \
  --output output/scenarios/three_minute_run.csv
```

## 输入文件

### 配置文件

示例见 `config/default_config.json`。

关键字段：

- `simulation.dt_s`
- `simulation.duration_s`
- `simulation.earth_radius_m`
- `simulation.nomoto_T_s`
- `simulation.nomoto_K`
- `simulation.speed_tau_s`
- `simulation.rudder_limit_deg`
- `initial_state.lat_deg`
- `initial_state.lon_deg`
- `initial_state.heading_deg`
- `initial_state.speed_mps`
- `engine_order_map`

### 命令文件

示例见 `config/sample_commands.csv`。

CSV 格式：

```text
time_s,type,value
0.0,engine,ahead3
5.0,rudder,10
15.0,rudder,0
20.0,engine,full
```

说明：

- `type=engine` 时，`value` 是车令字符串
- `type=rudder` 时，`value` 是舵角，单位为度

## 输出格式

输出 CSV 表头如下：

```text
time_s,lat_deg,lon_deg,heading_deg,speed_mps,yaw_rate_deg_s
```

示例：

```text
0.000000,24.000000,120.000000,0.000000,0.000000,0.000000
0.100000,24.000000,120.000000,0.000000,0.037500,0.000000
0.200000,24.000000,120.000000,0.000000,0.074531,0.000000
```

## 测试

执行：

```bash
ctest --test-dir build --output-on-failure
```

当前测试覆盖：

- 速度响应
- 航向响应
- 地理坐标换算
- 命令调度顺序
- 配置加载
- 日志输出
- `--output` 文件写入

## 绘图

系统中如果已有 `matplotlib`，可以直接运行：

```bash
mkdir -p .cache/matplotlib
  MPLCONFIGDIR=.cache/matplotlib python3 scripts/plot_simulation.py \
    --input output/scenarios/three_minute_run.csv \
    --commands config/three_minute_commands.csv \
    --output output/plots/three_minute_run_map.png \
    --title "Three Minute Ship Motion Scenario"
```

脚本会生成 5 个图层：

- 轨迹图
- 航向曲线
- 航速曲线
- 转首角速度曲线
- 命令时间线

其中首个轨迹图层现在采用离线“简易地图底图”样式：

- 浅色海图风格背景
- 经纬网格
- 起点与终点标记
- 轨迹方向箭头
- 命令触发点标记

说明：

- 这不是在线瓦片地图，也不依赖网络服务。
- 它本质上是在经纬度坐标上叠加简化地图样式，适合快速查看轨迹走势。

## 已知边界

- 不包含 3-DOF 或 MMG 模型
- 不包含风浪流扰动
- 不包含舵机速率限制
- 不适用于大范围高精度地理导航计算

## 后续建议

- 增加舵机一阶动态
- 将 `K`、`T` 扩展为随航速变化
- 增加环境扰动
- 增加更完整的集成测试和结果基线

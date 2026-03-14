# 基于一阶 Nomoto 的舰艇二维平面机动仿真器技术方案

## 1. 文档目的

本文档用于将 `draft.md` 中的最小可运行思路落地为可实现、可测试、可扩展的 C++ 技术方案。目标是指导首版 CLI 程序实现，而不是定义通用舰船操纵仿真平台。

## 2. 建设目标

实现一个最小 C++ 命令行程序，基于一阶 Nomoto 航向模型和一阶速度响应模型，按固定时间步推进舰艇状态，并持续输出日志。

### 2.1 功能目标

- 支持设置初始纬度、经度、航向、航速。
- 支持设置舵令，直接映射为目标舵角。
- 支持设置车令，映射为目标航速。
- 支持舵令与车令同时生效。
- 支持固定步长仿真推进。
- 支持持续输出时间、经纬度、航向、航速。

### 2.2 非目标

首版明确不做以下内容：

- 3-DOF 或 6-DOF 船舶运动模型
- MMG 模型
- 推进器、阻力、螺旋桨、舵机的高保真建模
- 风、浪、流等环境扰动
- 地理大范围高精度大地测量
- 图形界面
- 自动驾驶、航迹跟踪、控制律闭环

## 3. 总体实现原则

- 简单优先：只保留实现目标所需的最少状态和模块。
- 单位统一：内部统一使用 SI 单位和弧度制。
- 可重复：同样输入应产生一致输出。
- 可验证：每个子模型必须能做独立单元测试。
- 可扩展：后续升级舵机模型、事件调度、环境扰动时不推翻现有结构。

## 4. 物理与数学模型

## 4.1 状态定义

### 对外输出状态

- `lat_deg`：纬度，单位 `deg`
- `lon_deg`：经度，单位 `deg`
- `heading_deg`：航向角，单位 `deg`
- `speed_mps`：航速，单位 `m/s`

### 对内状态

- `x_m`：局部东向位移，单位 `m`
- `y_m`：局部北向位移，单位 `m`
- `heading_rad`：内部航向角，单位 `rad`
- `yaw_rate_rps`：转首角速度 `r`，单位 `rad/s`
- `rudder_cmd_rad`：当前舵令，单位 `rad`
- `u_cmd_mps`：当前车令映射后的目标航速，单位 `m/s`

说明：

- 经纬度不是直接积分变量，积分在局部切平面中完成。
- `x_m`、`y_m` 相对初始点累计，最后换算为经纬度。
- 内部航向建议归一化到 `[0, 2π)`，输出时转换为角度制。

## 4.2 航向模型

采用一阶 Nomoto 模型：

```text
T * r_dot + r = K * delta
r_dot = (K * delta - r) / T
psi_dot = r
```

参数说明：

- `T`：转向时间常数，单位 `s`
- `K`：舵效增益，单位可视为 `1/s`
- `delta`：舵角，单位 `rad`
- `r`：转首角速度，单位 `rad/s`
- `psi`：航向角，单位 `rad`

建模假设：

- 舵角直接等于舵令，不建模舵机动态。
- `K`、`T` 为常数，不随航速变化。
- 首版只刻画平面转向惯性，不追求实船辨识精度。

## 4.3 航速模型

采用一阶速度响应模型：

```text
tau_u * u_dot + u = u_cmd
u_dot = (u_cmd - u) / tau_u
```

参数说明：

- `u`：当前航速，单位 `m/s`
- `u_cmd`：车令映射目标航速，单位 `m/s`
- `tau_u`：速度响应时间常数，单位 `s`

建模假设：

- 不区分推进与阻力来源。
- 不考虑转向对航速的耦合衰减。
- 支持负值航速，用于简单倒车。

## 4.4 平面位置更新

局部平面速度分解：

```text
x_dot = u * cos(psi)
y_dot = u * sin(psi)
```

其中：

- `x_m` 对应东向位移
- `y_m` 对应北向位移

## 4.5 经纬度换算

以初始点为参考原点，采用局部切平面近似：

```text
lat_rad = lat0_rad + y_m / earth_radius_m
lon_rad = lon0_rad + x_m / (earth_radius_m * cos(lat0_rad))
```

适用边界：

- 适合局部、小范围操纵仿真。
- 若场景跨越大范围航段或高纬度区域，应升级为更严格的地理坐标换算模型。

## 5. 数值积分方案

首版使用显式欧拉法，理由如下：

- 模型为一阶常微分方程，结构简单。
- `dt = 0.1s` 时对首版原型足够稳定。
- 便于调试和验证每一步状态演化。

单步推进流程如下：

### Step 1. 读取当前控制命令

- 将舵令转换为 `rudder_cmd_rad`
- 将车令查表转换为 `u_cmd_mps`

### Step 2. 更新航速

```text
u_dot = (u_cmd_mps - speed_mps) / tau_u
speed_mps = speed_mps + u_dot * dt
```

### Step 3. 更新转首角速度

```text
r_dot = (K * rudder_cmd_rad - yaw_rate_rps) / T
yaw_rate_rps = yaw_rate_rps + r_dot * dt
```

### Step 4. 更新航向

```text
heading_rad = heading_rad + yaw_rate_rps * dt
heading_rad = normalize_angle_0_2pi(heading_rad)
```

### Step 5. 更新平面位置

```text
x_m = x_m + speed_mps * cos(heading_rad) * dt
y_m = y_m + speed_mps * sin(heading_rad) * dt
```

### Step 6. 计算输出经纬度

- 根据 `x_m`、`y_m` 与初始点换算 `lat_deg`、`lon_deg`
- 保留内部弧度值，输出时转换为度

### 数值稳定性约束

- `T > 0`
- `tau_u > 0`
- `dt > 0`
- 建议 `dt <= min(T, tau_u) / 10`

若参数非法，程序应直接报错并终止。

## 6. 输入设计

## 6.1 初始化输入

首版建议通过配置文件或命令行参数传入以下初始值：

- `initial_lat_deg`
- `initial_lon_deg`
- `initial_heading_deg` 或 `initial_heading_rad`
- `initial_speed_mps`

为降低接口歧义，首版建议对外统一使用：

- 经纬度：`deg`
- 航向：`deg`
- 航速：`m/s`

程序内部统一转换为弧度。

## 6.2 运行时输入

运行时控制采用“离散事件表”方式，便于 CLI 实现与回放测试。

事件字段建议为：

- `time_s`：命令生效时间
- `type`：`rudder` 或 `engine`
- `value`：舵角值或车令字符串

示例：

```text
0.0,engine,ahead3
5.0,rudder,10
20.0,rudder,0
35.0,engine,stop
```

解释：

- `rudder` 类型的 `value` 取角度制，正右负左。
- `engine` 类型的 `value` 为车令枚举名。

## 7. 控制语义设计

## 7.1 舵令

舵令采用角度直接输入，单位为 `deg`。程序内部转为 `rad`。

推荐范围：

- `[-35, +35] deg`

处理策略：

- 若超出范围，默认执行夹紧并记录警告日志。
- 首版不实现舵机速率限制。

## 7.2 车令

车令不直接输入目标速度，而是通过配置映射。

默认映射表示例：

| 车令 | 目标航速 m/s |
| --- | --- |
| `stop` | `0.0` |
| `ahead1` | `1.0` |
| `ahead2` | `2.0` |
| `ahead3` | `3.0` |
| `ahead4` | `4.0` |
| `ahead5` | `5.0` |
| `full` | `6.5` |
| `astern3` | `-1.5` |

实现要求：

- 映射表可配置。
- 未知车令必须报错并终止仿真。

## 8. 软件架构设计

## 8.1 模块划分

建议拆分为以下模块：

### 1. `types`

负责定义状态、配置、命令事件等基础数据结构。

### 2. `model`

负责舰艇状态推进，是核心算法模块。

### 3. `scenario`

负责读取命令事件表并在仿真时刻触发。

### 4. `logger`

负责将状态序列输出为标准文本或 CSV。

### 5. `app`

负责参数解析、对象装配、仿真循环与错误处理。

## 8.2 推荐目录结构

```text
ship-motion-sim/
├─ CMakeLists.txt
├─ src/
│  ├─ main.cpp
│  ├─ app/
│  │  └─ simulation_app.cpp
│  ├─ model/
│  │  ├─ simple_nomoto_ship_model.h
│  │  └─ simple_nomoto_ship_model.cpp
│  ├─ scenario/
│  │  ├─ command_event.h
│  │  ├─ command_schedule.h
│  │  └─ command_schedule.cpp
│  ├─ logger/
│  │  ├─ state_logger.h
│  │  └─ state_logger.cpp
│  └─ common/
│     ├─ math_utils.h
│     └─ units.h
├─ config/
│  ├─ default_config.json
│  └─ sample_commands.csv
└─ tests/
   ├─ test_model.cpp
   ├─ test_schedule.cpp
   └─ test_geo.cpp
```

如果当前只做极简版本，也可以先只保留：

- `main.cpp`
- `simple_nomoto_ship_model.h`
- `simple_nomoto_ship_model.cpp`

但推荐从一开始就把调度与日志模块独立出来，避免后续重构。

## 9. 核心数据结构

## 9.1 仿真配置

```cpp
struct SimulationConfig {
    double dt_s;
    double duration_s;
    double earth_radius_m;
    double nomoto_T_s;
    double nomoto_K;
    double speed_tau_s;
    double rudder_limit_deg;
};
```

## 9.2 初始状态

```cpp
struct InitialState {
    double lat_deg;
    double lon_deg;
    double heading_deg;
    double speed_mps;
};
```

## 9.3 输出状态

```cpp
struct ShipState {
    double time_s;
    double lat_deg;
    double lon_deg;
    double heading_deg;
    double speed_mps;
    double yaw_rate_deg_s;
};
```

## 9.4 命令事件

```cpp
enum class CommandType {
    Rudder,
    Engine
};

struct CommandEvent {
    double time_s;
    CommandType type;
    std::string value;
};
```

说明：

- `rudder` 事件可先保存在 `value` 中，解析时转为 `double`。
- 若追求类型安全，可拆成 `RudderCommandEvent` 和 `EngineCommandEvent`。

## 10. 核心类设计

## 10.1 `SimpleNomotoShipModel`

职责：

- 保存舰艇内部状态
- 响应当前控制命令
- 执行一步数值积分
- 提供可输出的状态快照

建议接口：

```cpp
class SimpleNomotoShipModel {
public:
    explicit SimpleNomotoShipModel(const SimulationConfig& config);

    void setInitialState(const InitialState& state);
    void setRudderCommandDeg(double rudder_deg);
    void setEngineCommand(const std::string& order);
    void setEngineOrderMapping(const std::unordered_map<std::string, double>& mapping);

    void step(double dt_s);
    ShipState getState(double sim_time_s) const;

private:
    void validateConfig() const;
    void updateSpeed(double dt_s);
    void updateYawRate(double dt_s);
    void updateHeading(double dt_s);
    void updatePosition(double dt_s);
    double clampRudderRad(double rudder_rad) const;
    double normalizeHeadingRad(double heading_rad) const;
    std::pair<double, double> toLatLonDeg() const;
};
```

## 10.2 `CommandSchedule`

职责：

- 按时间排序持有事件表
- 在每个仿真步前提取当前时刻应生效的所有事件

建议接口：

```cpp
class CommandSchedule {
public:
    explicit CommandSchedule(std::vector<CommandEvent> events);
    std::vector<CommandEvent> popReadyEvents(double sim_time_s);
    bool empty() const;
};
```

设计说明：

- 事件表按 `time_s` 升序排序。
- 当多个事件时间相同，按输入顺序依次执行。
- 同一时刻允许先改车令再改舵令，两者互不冲突。

## 10.3 `StateLogger`

职责：

- 输出 CSV 表头
- 写出每个时间步状态
- 控制数值格式和精度

建议输出格式：

```text
time_s,lat_deg,lon_deg,heading_deg,speed_mps,yaw_rate_deg_s
0.0,24.000000,120.000000,0.000000,0.000000,0.000000
0.1,24.000000,120.000003,0.000000,0.037500,0.000000
```

## 11. 仿真主循环设计

主循环建议流程：

```text
1. 加载配置
2. 加载车令映射表
3. 加载初始状态
4. 加载命令事件表
5. 初始化模型
6. 输出表头和 t=0 状态
7. for time from dt to duration step dt:
8.     应用所有 time <= 当前时刻 的命令事件
9.     model.step(dt)
10.    输出当前状态
11. 结束
```

关键约束：

- 事件触发应在该步积分之前执行。
- 首行日志必须输出初始状态，便于与输入核对。
- 日志时间应以累积仿真时间为准，而不是系统时钟。

## 12. 配置方案

推荐使用 JSON 作为静态配置格式，CSV 作为命令时间表格式。

## 12.1 JSON 配置示例

```json
{
  "simulation": {
    "dt_s": 0.1,
    "duration_s": 120.0,
    "earth_radius_m": 6378137.0,
    "nomoto_T_s": 10.0,
    "nomoto_K": 0.08,
    "speed_tau_s": 8.0,
    "rudder_limit_deg": 35.0
  },
  "initial_state": {
    "lat_deg": 24.0,
    "lon_deg": 120.0,
    "heading_deg": 0.0,
    "speed_mps": 0.0
  },
  "engine_order_map": {
    "stop": 0.0,
    "ahead1": 1.0,
    "ahead2": 2.0,
    "ahead3": 3.0,
    "ahead4": 4.0,
    "ahead5": 5.0,
    "full": 6.5,
    "astern3": -1.5
  }
}
```

## 12.2 命令文件示例

```text
time_s,type,value
0.0,engine,ahead3
5.0,rudder,10
20.0,rudder,0
35.0,engine,full
60.0,rudder,-15
80.0,engine,stop
```

## 13. CLI 交互设计

建议支持以下运行方式：

```bash
./ship_motion_sim --config config/default_config.json --commands config/sample_commands.csv
```

建议参数：

- `--config`：JSON 配置路径
- `--commands`：命令时间表路径
- `--output`：可选，输出文件路径；缺省则写标准输出

错误处理要求：

- 参数缺失时打印用法说明并返回非零退出码。
- 配置文件打不开、字段缺失、字段非法时直接报错。
- 命令表格式不正确时直接报错。

## 14. 日志与可观测性设计

日志应分为两类：

### 1. 状态日志

用于仿真输出，建议写 CSV。

### 2. 运行日志

用于错误与警告，建议写标准错误输出。

需要输出的告警类型：

- 舵角被夹紧
- 未识别车令
- 配置参数非法
- 经纬度换算存在潜在高纬度风险

## 15. 边界条件与异常处理

必须明确处理以下情况：

### 15.1 参数非法

- `T <= 0`
- `tau_u <= 0`
- `dt <= 0`
- `earth_radius_m <= 0`

处理方式：程序启动阶段直接失败。

### 15.2 高纬度问题

当 `abs(cos(lat0_rad))` 非常接近 0 时，`lon` 换算将不稳定。

处理策略：

- 若 `abs(cos(lat0_rad)) < 1e-6`，直接报错。

### 15.3 倒车

允许 `speed_mps < 0`，这会使位置按当前航向反向推进。

这是首版约定，不额外引入“倒车航向变化规则”。

### 15.4 空命令表

允许命令表为空。此时舰艇保持初始舵令和默认车令运行。

默认值建议为：

- `rudder_cmd_rad = 0`
- `u_cmd_mps = initial_speed_mps`

## 16. 测试与验证方案

## 16.1 单元测试

### 航速响应测试

给定：

- 初始速度 `0`
- `u_cmd = 3.0`
- `tau_u = 8.0`

验证：

- 多步推进后速度单调趋近 `3.0`
- 不应出现数值发散

### 航向响应测试

给定：

- 初始 `r = 0`
- 固定舵角 `10 deg`

验证：

- `yaw_rate` 单调趋向 `K * delta`
- `heading` 持续增长

### 经纬度换算测试

给定：

- `x_m = 0`
- `y_m = earth_radius_m * deg_to_rad(1.0)`

验证：

- 输出纬度应增加约 `1 deg`

### 舵角夹紧测试

给定舵令 `60 deg`，舵限 `35 deg`，验证内部实际舵角为 `35 deg`。

## 16.2 集成测试

### 直航场景

命令：

- `engine = ahead3`
- 舵角始终为 `0`

预期：

- 航向基本不变
- 经度或纬度按初始航向方向单调变化

### 定舵转向场景

命令：

- `engine = ahead3`
- `rudder = 10 deg`

预期：

- 航向持续变化
- 航迹出现曲线

### 复位场景

命令：

- `t=0` 加速
- `t=5` 右舵
- `t=20` 回中
- `t=40` 停车

预期：

- 航速先上升后趋近 0
- 航向在回中后继续短时变化，然后趋于稳定

## 16.3 验收标准

满足以下条件即可认为首版完成：

1. 能从配置加载初始状态和参数。
2. 能从命令表加载舵令与车令。
3. 车令作用下航速逐步逼近目标值。
4. 舵令作用下航向逐步变化，非瞬时跳变。
5. 船位随时间持续更新。
6. 能按固定步长稳定输出日志。
7. 非法输入能被检测并明确报错。

## 17. 性能与实现约束

首版性能目标很低，核心要求是稳定、清晰、易测：

- 单次仿真 1 万步以内应可瞬时完成。
- 不引入不必要的动态分配和复杂模板。
- 数值类型统一使用 `double`。
- 若引入第三方库，限制在参数解析或测试框架层面。

## 18. 后续扩展路线

建议扩展顺序如下：

1. 增加舵机一阶动态 `tau_delta * delta_dot + delta = delta_cmd`
2. 将 `K`、`T` 设计为与航速相关的参数
3. 引入风浪流扰动项
4. 升级为 3-DOF 平面操纵模型
5. 升级地理坐标换算精度
6. 提供实时控制输入和图形化显示

这样可以保证首版实现保持最小，同时为后续演进预留清晰边界。

## 19. 默认参数建议

首版建议默认值如下：

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `dt_s` | `0.1` | 仿真步长 |
| `duration_s` | `120.0` | 默认仿真时长 |
| `earth_radius_m` | `6378137.0` | 地球半径 |
| `nomoto_T_s` | `10.0` | 转向时间常数 |
| `nomoto_K` | `0.08` | 舵效增益 |
| `speed_tau_s` | `8.0` | 速度响应时间常数 |
| `rudder_limit_deg` | `35.0` | 最大舵角绝对值 |

说明：

- 这些不是实船标定值，而是原型参数。
- 首版目标是行为合理，不是物理精确。

## 20. 实施结论

本方案将问题严格收敛为“固定步长、平面二维、最小控制输入、最小状态输出”的工程实现问题。核心复杂度仅集中在：

- 一阶 Nomoto 航向响应
- 一阶速度逼近
- 局部平面到经纬度的换算
- 命令调度与日志输出

按本方案实现后，即可得到一版真正可运行、可回放、可测试的舰艇机动仿真 CLI 原型，并为下一阶段的舵机动态、环境扰动和更高阶操纵模型升级提供稳定基础。

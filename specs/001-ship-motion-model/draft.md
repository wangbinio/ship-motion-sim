
你现在要的不是“通用舰船仿真框架”，而是一个**最小可运行模型**：

- 输入：初始经纬度、初始航向、初始航速、舵令、车令
- 状态：经纬度、航向、航速
- 模型：**一阶 Nomoto 航向模型 + 一个一阶速度响应模型**
- 输出：按固定时间步打印日志

这版完全够做“船动起来”。

------

# 一、这版 PRD 的一句话定义

实现一个**基于一阶 Nomoto 模型的二维舰艇平面机动仿真器**，接收舵令和车令，按固定时间步更新舰艇的**经纬度、航向、航速**，并持续输出日志。

------

# 二、这版只保留 4 个状态量

内部最少只需要这几个：

- `lat_deg`：纬度
- `lon_deg`：经度
- `heading_rad`：航向角，范围建议归一化到 `[0, 2π)`
- `speed_mps`：航速

为了计算方便，再加一个内部状态：

- `yaw_rate_rps`：转首角速度 `r`

也就是说：

## 对外输出

- 经纬度
- 航向
- 航速

## 对内维护

- 再多保存一个 `r`

因为一阶 Nomoto 最自然的状态就是转首角速度。常见写法是
**`T \* r_dot + r = K \* delta`**，其中 `r = ψ_dot`，`delta` 是舵角。([ResearchGate](https://www.researchgate.net/profile/Swarup-Das-2/publication/277497099_Ships_Steering_Autopilot_Design_by_Nomoto_Model/links/556c06c808aec2268303817b/Ships-Steering-Autopilot-Design-by-Nomoto-Model.pdf?utm_source=chatgpt.com))

------

# 三、模型结构

这版拆成两个子模型就够了：

## 1. 航向模型：一阶 Nomoto

用最经典的一阶形式：

[
T \dot r + r = K \delta
]

[
\dot \psi = r
]

其中：

- `delta`：当前舵角，单位建议用 rad 或 deg，但内部统一成 rad
- `r`：转首角速度
- `psi`：航向角
- `T`：时间常数
- `K`：舵效增益

这个模型表达的是：舵角不会让船瞬间转起来，而是经过一个一阶惯性过程逐步建立转首角速度。([ResearchGate](https://www.researchgate.net/profile/Swarup-Das-2/publication/277497099_Ships_Steering_Autopilot_Design_by_Nomoto_Model/links/556c06c808aec2268303817b/Ships-Steering-Autopilot-Design-by-Nomoto-Model.pdf?utm_source=chatgpt.com))

------

## 2. 航速模型：一阶响应

车令先不要搞复杂推进阻力，只做一个最小模型：

[
\tau_u \dot u + u = u_{cmd}
]

其中：

- `u`：当前航速
- `u_cmd`：车令对应的目标航速
- `tau_u`：速度响应时间常数

意思是：

- 两进三 → 不直接瞬间变成某个速度
- 而是逐渐逼近目标速度

这虽然不是 Nomoto，但非常适合当前阶段，因为你只想让“车令能让船加减速”。这部分是一个工程上的简化设计，不是标准 Nomoto 本体。

------

## 3. 位置更新

在局部平面上：

[
\dot x = u \cos \psi
]
[
\dot y = u \sin \psi
]

再把局部东-北位移转换回经纬度。

如果活动范围不大，可以直接用局部切平面近似：

- `north = y`
- `east = x`

然后：

[
\Delta lat = \frac{north}{R}
]

[
\Delta lon = \frac{east}{R \cos(lat_0)}
]

其中 `R` 取地球半径近似值。这个“小范围局部平面近似”对训练原型很合适。
如果后面要大范围航迹，再升级成更严格的大地测量转换。

------

# 四、最小输入集

你只需要这些输入：

## 初始化输入

- 初始纬度 `lat0`
- 初始经度 `lon0`
- 初始航向 `heading0_rad`
- 初始航速 `speed0_mps` 或 `speed0_knots`

## 运行时输入

- 舵令：目标舵角
- 车令：目标航速档位
- 支持舵令和车令同时控制

------

# 五、最小控制语义

## 1. 舵令

直接映射成目标舵角就行：

- 左30 → `delta_cmd = -30 deg`
- 左10 → `delta_cmd = -10 deg`
- 左5 → `delta_cmd = -5 deg`
- 舵正 → `delta_cmd = 0 deg`
- 右5 → `delta_cmd = +5 deg`
- 右10 → `delta_cmd = +10 deg`
- 右30 → `delta_cmd = +30 deg`

首版可以直接设定：

- `delta = delta_cmd`

也就是**暂不做舵机迟滞**。
这样更简单。

如果你想稍微真实一点，再加一条：

[
\tau_\delta \dot \delta + \delta = \delta_{cmd}
]

但我建议这版先不加。

------

## 2. 车令

车令只映射到目标航速：

例如：

- 停车 → `0 m/s`
- 两进一 → `1.0 m/s`
- 两进二 → `2.0 m/s`
- 两进三 → `3.0 m/s`
- 两进四 → `4.0 m/s`
- 全速 → `6.0 m/s`
- 两退三 → `-1.5 m/s`

这些值先别追求真实，先做成**配置表**就够了。

------

# 六、你真正需要的参数，只有很少几个

这版别搞一堆参数，只要下面这些。

## Nomoto 参数

- `T`：转向时间常数
- `K`：舵效增益

## 速度响应参数

- `tau_u`：速度一阶响应时间常数

## 地理参数

- `earth_radius_m`：地球半径，默认 6378137 或 6371000 都行

## 仿真参数

- `dt`：步长，比如 0.1 s

## 车令映射表

- 每个车令对应一个目标航速

总共其实就这几类。

------

# 七、推荐的最小数学流程

每个时间步 `dt` 做下面 5 步：

## Step 1：处理舵令

把当前命令转成 `delta_cmd`

首版直接：

- `delta = delta_cmd`

## Step 2：处理车令

查表得到目标速度 `u_cmd`

更新速度：

[
\dot u = \frac{u_{cmd} - u}{\tau_u}
]

[
u_{k+1} = u_k + \dot u \cdot dt
]

## Step 3：更新 Nomoto 转向

一阶 Nomoto：

[
\dot r = \frac{K \delta - r}{T}
]

[
r_{k+1} = r_k + \dot r \cdot dt
]

[
\psi_{k+1} = \psi_k + r_{k+1} \cdot dt
]

然后把 `psi` 归一化到 `[0, 2π)`。

## Step 4：更新平面位置

[
x_{k+1} = x_k + u_{k+1}\cos(\psi_{k+1})dt
]

[
y_{k+1} = y_k + u_{k+1}\sin(\psi_{k+1})dt
]

## Step 5：把平面位移转回经纬度

假设 `x = east`, `y = north`：

[
lat = lat_0 + \frac{y}{R}
]

[
lon = lon_0 + \frac{x}{R \cos(lat_0)}
]

注意这里计算时纬度经度要用弧度，最后输出再转成度。

------

# 八、建议的最小项目规格

Codex 现在只需要实现这些：

## 一个类：`SimpleNomotoShipModel`

字段：

- `lat0_rad`
- `lon0_rad`
- `x_m`
- `y_m`
- `heading_rad`
- `speed_mps`
- `yaw_rate_rps`
- `rudder_cmd_rad`
- `engine_order`
- `u_cmd_mps`
- `T`
- `K`
- `tau_u`
- `dt`

方法：

- `setInitialState(lat_deg, lon_deg, heading_rad, speed_mps)`
- `setRudderCommand(delta_deg)`
- `setEngineCommand(order)`
- `step()`
- `getState()`

------

# 九、最小输出格式

日志每 0.1s 或 0.5s 打一行就行：

```text
time_s, lat_deg, lon_deg, heading_rad, speed_mps
0.0, 24.000000, 120.000000, 0.000000, 0.000000
0.1, 24.000001, 120.000000, 0.000000, 0.120000
0.2, 24.000002, 120.000000, 0.000015, 0.230000
...
```

这就已经满足你现在的需求了。

------

# 十、建议你给 Codex 的简化实现要求

你可以直接这样下任务：

## 实现目标

请实现一个最小 C++ CLI 程序，使用**一阶 Nomoto 航向模型 + 一阶速度响应模型**模拟舰艇二维平面运动。程序支持：

- 设置初始纬度、经度、航向、航速
- 设置舵令（目标舵角）
- 设置车令（目标航速档位）
- 按固定时间步推进
- 输出经纬度、航向、航速日志

## 数学模型

使用：

[
\dot r = (K\delta - r)/T
]
[
\dot \psi = r
]
[
\dot u = (u_{cmd} - u)/\tau_u
]
[
\dot x = u\cos\psi
]
[
\dot y = u\sin\psi
]

并把 `x,y` 转换成经纬度输出。

## 暂不实现

- Qt
- 复杂阻力模型
- 3-DOF
- MMG
- 舵机速率限制
- 风浪流

------

# 十一、首版建议参数

先给一套能“看起来正常”的默认值：

- `T = 10.0`
- `K = 0.08`
- `tau_u = 8.0`
- `dt = 0.1`

车令映射先这样：

- `stop = 0.0`
- `ahead1 = 1.0`
- `ahead2 = 2.0`
- `ahead3 = 3.0`
- `ahead4 = 4.0`
- `ahead5 = 5.0`
- `full = 6.5`
- `astern3 = -1.5`

这不是实船标定值，只是原型默认值。

------

# 十二、这版 PRD 的最小验收标准

做到下面这些，就算完成：

1. 能设置初始点 `(24, 120)`、初始航向、初始航速
2. 发出车令后，航速会逐渐变化
3. 发出舵令后，航向会逐渐变化
4. 支持同时设置车令和舵令
5. 船位会持续变化
6. 能持续输出：
   - 纬度
   - 经度
   - 航向
   - 航速
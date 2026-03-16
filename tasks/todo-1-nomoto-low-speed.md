# 本次任务计划

- [x] 核对当前工作区差异，确认首个提交仅包含现有 Nomoto 航迹修正与测试
- [x] 提交当前版本，形成低速舵效修改前的基线提交
- [x] 设计并实现低速时舵效随航速衰减的简化方案，避免零速原地转向
- [x] 补充测试，覆盖零速和低速下的转首边界行为
- [x] 执行构建与测试验证，并回写 Review

# Review

- 已先提交基线版本 `982ec9e fix(model): align nomoto track with navigation heading`，固定低速舵效修改前的航迹语义修正结果。
- 已在 [simple_nomoto_ship_model.cpp](/home/sun/projects/cpp/ship-motion-sim/src/model/simple_nomoto_ship_model.cpp) 为 Nomoto 舵效引入按 `abs(speed_mps) / rudder_full_effect_speed_mps` 线性缩放的简化模型，零速时不再仅因打舵产生新的转首角速度，达到阈值航速后恢复原有满舵效。
- 已在 [types.h](/home/sun/projects/cpp/ship-motion-sim/src/common/types.h) 与 [xml_config_loader.cpp](/home/sun/projects/cpp/ship-motion-sim/src/config/xml_config_loader.cpp) 增加 `rudder_full_effect_speed_mps` 配置项，并保持 XML 加载对旧配置的向后兼容，缺省值为 `1.0` m/s。
- 已更新 [default_config.xml](/home/sun/projects/cpp/ship-motion-sim/config/default_config.xml) 与测试夹具中的模型参数，确保默认场景和基线样例显式携带该阈值。
- 已在 [test_main.cpp](/home/sun/projects/cpp/ship-motion-sim/tests/test_main.cpp) 新增“零速打舵不从静止造出转首”和“低速舵效弱于巡航速度”两项断言，并保留原有航迹方向测试。
- 已执行 `cmake --build build -j4`、`ctest --test-dir build --output-on-failure` 和 `./build/ship_motion_sim_tests`，结果通过。

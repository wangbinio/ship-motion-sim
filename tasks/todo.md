# 本次任务计划

- [x] 检查地图航迹左右反向问题，确认模型内部航向角与局部东-北坐标约定不一致
- [x] 修正 `SimpleNomotoShipModel` 的航向更新与位置积分语义，使其符合海图/航海约定
- [x] 补充并更新测试，覆盖右舵/左舵对应的航迹横向变化
- [x] 执行构建与测试验证，并回写 Review

# Review

- 已在 [simple_nomoto_ship_model.cpp](/home/sun/projects/cpp/ship-motion-sim/src/model/simple_nomoto_ship_model.cpp) 将位置积分改为航海语义下的东-北分解：`0°` 指北、顺时针为正，因此东向位移使用 `sin(heading)`，北向位移使用 `cos(heading)`，修复了地图上左右转向与航迹横向偏移相反的问题。
- 已在 [simple_nomoto_ship_model.h](/home/sun/projects/cpp/ship-motion-sim/src/model/simple_nomoto_ship_model.h) 为函数声明补充中文注释，明确舵令、车令、航向和局部坐标的职责，便于后续 review。
- 已在 [test_main.cpp](/home/sun/projects/cpp/ship-motion-sim/tests/test_main.cpp) 新增 `heading=0` 直航向北、右舵向东偏、左舵向西偏三项断言，锁定本次修复的坐标系约定。
- 已执行 `cmake --build build -j4`、`ctest --test-dir build --output-on-failure` 和 `./build/ship_motion_sim_tests`，结果通过。

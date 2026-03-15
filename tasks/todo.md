# 本次任务计划

- [x] 确认本轮仅撤销自绘标题栏相关改动，保留现有内容区样式与资源加载方式
- [x] 回退 `MainWindow`、`main_window.qss` 和测试中的自绘标题栏逻辑，恢复系统标题栏
- [x] 执行构建验证并回写 Review

# Review

- 已从 [main_window.cpp](/home/sun/projects/cpp/ship-motion-sim/src/gui/main_window.cpp:75) 回退无边框与自绘标题栏逻辑，窗口重新使用系统标题栏，同时保留置顶、透明背景、图标和现有内容区布局。
- 已从 [main_window.qss](/home/sun/projects/cpp/ship-motion-sim/src/res/main_window.qss:1) 移除 `titleBar`、`titleLabel` 和标题栏按钮样式块，未改动你手调后的内容区样式规则。
- 已将 [test_main.cpp](/home/sun/projects/cpp/ship-motion-sim/tests/test_main.cpp:182) 的 GUI 冒烟测试恢复为系统标题栏断言，不再检查无边框和自绘标题栏控件。
- 已执行 `cmake --build build -j4` 与 `ctest --test-dir build --output-on-failure`，结果通过。

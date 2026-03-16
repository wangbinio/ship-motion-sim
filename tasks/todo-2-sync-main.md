# 本次任务计划

- [x] 在独立 main 工作树中同步 simplify 的模型与配置相关提交，避免影响当前 simplify 工作区未提交改动
- [x] 在 main 上 cherry-pick `982ec9e` 与 `3db2d06`
- [x] 执行构建与测试验证同步结果
- [x] 回写 Review，记录同步结果与验证命令

# Review

- 已在独立工作树 `/home/sun/projects/cpp/ship-motion-sim-main-sync` 上完成同步，避开当前 `simplify` 工作区里未提交的 `.gitignore` 与测试夹具删除改动。
- 已将 `982ec9e fix(model): align nomoto track with navigation heading` 和 `3db2d06 feat(model): reduce rudder authority at low speed` 依次 cherry-pick 到 `main`，并按 `main` 现有测试骨架解决了 `tasks/todo.md`、`config/default_config.xml` 与 `tests/test_main.cpp` 的冲突。
- 已在 `main` 上同步模型、配置结构、XML 读写、默认配置、测试夹具，以及低速舵效相关的单元测试。
- 主干的端到端基线因模型输出变化而失效，已刷新 `tests/baselines/baseline_straight_expected.csv` 与 `tests/baselines/baseline_turn_expected.csv` 以匹配新模型行为。
- 已执行 `cmake -S . -B build`、`cmake --build build -j4`、`ctest --test-dir build --output-on-failure` 与 `./build/ship_motion_sim_tests`，结果通过。

# 本次任务计划

- [x] 审查 `CMakeLists.txt` 与 `src/` 目录，确认阶段 2 后的冗余 target 和废弃源码
- [x] 删除无引用的旧 JSON 配置路径代码与重复 CLI target，收敛到当前 XML/GUI/CLI 实际需要的最小集合
- [x] 同步修正文档中与构建产物、入口命名相关的引用，避免继续指向被清理内容
- [x] 重新构建并运行测试，验证清理没有引入回归
- [x] 整理变更并创建 git commit

# Review

- 已从 `src/` 中删除无引用的旧 JSON 配置路径代码：`src/config/config_loader.*` 与 `src/common/simple_json.*`。
- 已从 `CMakeLists.txt` 中移除重复的 CLI target `ship_motion_sim_cli`，保留 `ship_motion_sim` 作为唯一 CLI 入口。
- 已从 `CMakeLists.txt` 中移除未实际使用的 `Qt5::Test` 依赖，当前测试仍通过 `QApplication` 和项目库完成。
- 已同步更新 `README.md` 与 `architecture.md` 中的构建产物说明，避免继续引用已删除的重复 target。
- 已执行 `cmake -S . -B build`、`cmake --build build -j4` 与 `ctest --test-dir build --output-on-failure`，结果通过。
- 本次提交不会纳入 `control.png` 与 `phase-2-draft.md`，它们继续保留为工作区输入素材。 

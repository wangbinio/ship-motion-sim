cmake_minimum_required(VERSION 3.16)

project(ship_motion_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(
    ship_motion_sim_lib
    src/config/config_loader.cpp
    src/common/simple_json.cpp
    src/logger/state_logger.cpp
    src/model/simple_nomoto_ship_model.cpp
    src/scenario/command_schedule.cpp
    src/app/simulation_app.cpp
)

target_include_directories(ship_motion_sim_lib PUBLIC src)

add_executable(ship_motion_sim src/main.cpp)
target_link_libraries(ship_motion_sim PRIVATE ship_motion_sim_lib)

enable_testing()

add_executable(ship_motion_sim_tests tests/test_main.cpp)
target_link_libraries(ship_motion_sim_tests PRIVATE ship_motion_sim_lib)
target_compile_definitions(
    ship_motion_sim_tests PRIVATE
    SHIP_SIM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME ship_motion_sim_tests COMMAND ship_motion_sim_tests)

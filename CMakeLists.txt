cmake_minimum_required(VERSION 3.16)

project(ship_motion_sim LANGUAGES CXX)

list(APPEND CMAKE_PREFIX_PATH "/home/sun/Qt5.12.12/5.12.12/gcc_64")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Xml)

add_library(
    ship_motion_sim_lib
    src/app/batch_simulation_runner.cpp
    src/app/plot_runner.cpp
    src/app/realtime_simulation_controller.cpp
    src/app/simulation_app.cpp
    src/app/simulation_session.cpp
    src/config/xml_config_loader.cpp
    src/logger/state_logger.cpp
    src/model/simple_nomoto_ship_model.cpp
    src/scenario/command_schedule.cpp
    src/scenario/csv_command_io.cpp
)

target_include_directories(ship_motion_sim_lib PUBLIC src)
target_link_libraries(ship_motion_sim_lib PUBLIC Qt5::Core Qt5::Xml)

add_library(
    ship_motion_gui_lib
    src/gui/main_window.cpp
    src/gui/track_view_widget.cpp
)

target_include_directories(ship_motion_gui_lib PUBLIC src)
target_link_libraries(ship_motion_gui_lib PUBLIC ship_motion_sim_lib Qt5::Widgets)

add_executable(ship_motion_sim src/main.cpp)
target_link_libraries(ship_motion_sim PRIVATE ship_motion_sim_lib)

add_executable(ship_motion_sim_gui src/gui_main.cpp)
target_link_libraries(ship_motion_sim_gui PRIVATE ship_motion_gui_lib)

enable_testing()

add_executable(ship_motion_sim_tests tests/test_main.cpp)
target_link_libraries(ship_motion_sim_tests PRIVATE ship_motion_gui_lib)
target_compile_definitions(
    ship_motion_sim_tests PRIVATE
    SHIP_SIM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME ship_motion_sim_tests COMMAND ship_motion_sim_tests)

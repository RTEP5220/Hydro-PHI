cmake_minimum_required(VERSION 3.16)
project(smartflow_v6 VERSION 6.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-O2 -Wall -Wextra -Wno-unused-parameter)

find_library(LGPIO_LIB lgpio REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(I2C libi2c)

include_directories(include)

set(SOURCES
    src/main.cpp
    src/config.cpp
    src/tof_sensor.cpp
    src/load_cell.cpp
    src/ir_proximity.cpp
    src/actuator.cpp
    src/sensor_hub.cpp

)

/*LATER_ADD_UPON CONFIRM*
    src/logger.cpp
    src/worker.cpp
    src/thread_utils.cpp
*/


add_executable(smartflow_v6 ${SOURCES})
target_link_libraries(smartflow_v6 ${LGPIO_LIB} pthread i2c atomic)

enable_testing()
add_executable(smartflow_tests
    tests/test_logic.cpp
    tests/test_queue.cpp
    src/config.cpp
    src/worker.cpp
    src/logger.cpp
    src/thread_utils.cpp
)
target_link_libraries(smartflow_tests pthread atomic)
add_test(NAME SmartFlowV6Tests COMMAND smartflow_tests)

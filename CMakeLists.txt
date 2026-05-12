cmake_minimum_required(VERSION 3.16)
project(hphi VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(LGPIO_LIB lgpio REQUIRED)

add_executable(hphi
    src/main.cpp
    src/IRSensor.cpp
    src/Relay.cpp
    src/ToFSensor.cpp
    src/LoadCell.cpp
)

target_include_directories(hphi PRIVATE include)
target_link_libraries(hphi ${LGPIO_LIB} pthread)
cmake_minimum_required(VERSION 3.9)
project(libwatchpoint)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(3rdparty/zydis)

add_executable(example example/main.cpp)
target_link_libraries(example PRIVATE Zydis)
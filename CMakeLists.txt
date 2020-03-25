cmake_minimum_required(VERSION 3.9)
project(libwatchpoint VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 20)
include(GNUInstallDirs)

add_subdirectory(3rdparty/zydis)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS include/*)

add_library(watchpoint ${SOURCES} ${HEADERS})
target_link_libraries(watchpoint PRIVATE Zydis)
target_include_directories(watchpoint PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)


install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
install(
        TARGETS watchpoint
        EXPORT watchpoint-config
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(
        EXPORT watchpoint-config
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/watchpoint)


add_executable(example example/main.cpp)
target_link_libraries(example PRIVATE watchpoint)
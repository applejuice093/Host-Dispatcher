cmake_minimum_required(VERSION 3.21)

project(HostDispatcher VERSION 0.1.0 LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(host_dispatcher_core
    src/simulation.cpp
    src/simulation.h
)
target_include_directories(host_dispatcher_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

option(HOST_DISPATCHER_WARNINGS "Enable compiler warnings for the simulation core and tests" ON)
function(host_dispatcher_enable_warnings target)
    if(HOST_DISPATCHER_WARNINGS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endif()
endfunction()
host_dispatcher_enable_warnings(host_dispatcher_core)

option(HOST_DISPATCHER_BUILD_GUI "Build the Qt Widgets desktop application when Qt is available" ON)
if(HOST_DISPATCHER_BUILD_GUI)
    find_package(Qt6 6.2 QUIET COMPONENTS Widgets)
    if(Qt6_FOUND)
        set(CMAKE_AUTOMOC ON)
        qt_add_executable(host-dispatcher
            src/main.cpp
            src/main_window.cpp
            src/main_window.h
        )
        target_link_libraries(host-dispatcher PRIVATE host_dispatcher_core Qt6::Widgets)
    else()
        message(WARNING "Qt 6 Widgets was not found; building the simulation core and tests only.")
    endif()
endif()

enable_testing()
add_executable(simulation_tests tests/simulation_tests.cpp)
target_link_libraries(simulation_tests PRIVATE host_dispatcher_core)
host_dispatcher_enable_warnings(simulation_tests)
add_test(NAME simulation_tests COMMAND simulation_tests)

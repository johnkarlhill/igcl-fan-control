cmake_minimum_required(VERSION 3.15.0 FATAL_ERROR)

# Static CRT linking — no VC++ redistributable needed at runtime.
# Must be set BEFORE any target is defined.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(igcl-fan-control VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(igcl-fan-control
    src/main.cpp
    src/ini_reader.cpp
    src/fan_control.cpp
    src/gpu_discovery.cpp
    src/result_utils.cpp
    igcl/cApiWrapper.cpp
)

target_include_directories(igcl-fan-control PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/igcl
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    add_definitions(-DUNICODE -D_UNICODE)
    target_link_libraries(igcl-fan-control Advapi32.lib)
endif()

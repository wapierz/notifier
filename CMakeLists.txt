cmake_minimum_required(VERSION 3.28)


set(PROJECT_NAME notifier)


project(${PROJECT_NAME})

set (CMAKE_CXX_FLAGS "-std=c++23  -fconcepts-diagnostics-depth=3 ")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -ggdb ")

add_subdirectory(./external/curl)
add_subdirectory(./external/cxxopts)
include_directories (./include)



add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} curl cxxopts)


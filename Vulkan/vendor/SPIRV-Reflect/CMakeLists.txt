cmake_minimum_required(VERSION 3.1)
project(spirv-reflect)

set(CMAKE_CXX_STANDARD 14)

add_library(spirv-reflect-static STATIC ${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect.h ${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect.c)
target_include_directories(spirv-reflect-static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
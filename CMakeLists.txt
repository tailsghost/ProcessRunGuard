cmake_minimum_required (VERSION 3.8)

project(ProcessRunGuard LANGUAGES CXX)

add_library(ProcessRunGuard INTERFACE)

target_include_directories(ProcessRunGuard INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR} 
)
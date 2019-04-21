cmake_minimum_required(VERSION 3.6.0)

project(NetSplice)

# Enable and require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 64 bit file operations (ARM needs this)
add_definitions(-D_LARGEFILE64_SOURCE=1)

aux_source_directory(. SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})

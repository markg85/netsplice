cmake_minimum_required(VERSION 3.6.0)

project(NetSplice)

# Enable and require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 32 bit compiler additions
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_definitions(-D_LARGEFILE64_SOURCE=1)
    add_definitions(-D_FILE_OFFSET_BITS=64)
endif()

aux_source_directory(. SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})

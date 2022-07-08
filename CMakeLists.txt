cmake_minimum_required(VERSION 3.22)
project(threadpool)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)

add_executable(threadpool
        threadpool.hpp
        threadpool.cpp
        test.cpp)

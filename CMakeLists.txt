cmake_minimum_required(VERSION 3.30)
project(ffdb)

set(CMAKE_CXX_STANDARD 20)

add_executable(ffdb main.cpp
        FFDB.cpp
        FFDB.h
        buddy_allocator.cpp
        buddy_allocator.h
        typedefs.cpp
        typedefs.h
)

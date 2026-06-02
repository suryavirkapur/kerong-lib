cmake_minimum_required(VERSION 3.15)
project(kerong CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(KERONG_BUILD_PYTHON "Build Python bindings via pybind11" OFF)
option(KERONG_BUILD_NODE "Build Node.js bindings via node-addon-api" OFF)
option(KERONG_BUILD_STATIC "Build the C++ static core library" ON)

if(KERONG_BUILD_STATIC)
    add_subdirectory(src)
endif()

if(KERONG_BUILD_PYTHON)
    add_subdirectory(bindings/python)
endif()

if(KERONG_BUILD_NODE)
    add_subdirectory(bindings/node)
endif()

cmake_minimum_required(VERSION 3.28)

project(RingBufferBenchmarks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
if (TESTING)
    add_subdirectory(tests)
endif()

if (BENCH)
    add_subdirectory(bench)
endif()
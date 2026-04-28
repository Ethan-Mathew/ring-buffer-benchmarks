include(FetchContent)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.9.5
)

FetchContent_MakeAvailable(
    googlebenchmark
)

add_executable(Bench
    variants/staged/SPSCBufferStage6Bench.cpp
    #variants/BenchSPSCBufferBaseline.cpp
    #variants/BenchSPSCBufferBranchWrapIndices.cpp
    #variants/BenchSPSCBufferMonoBitmask.cpp
    #variants/BenchSPSCBufferMonoModulus.cpp
)

target_include_directories(Bench
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/bench
)

target_link_libraries(Bench
    PRIVATE
        benchmark::benchmark_main
)
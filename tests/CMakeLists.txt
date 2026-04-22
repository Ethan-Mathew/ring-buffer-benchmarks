include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(Tests
    MutexBufferTest.cpp
    MutexBufferStressTest.cpp
)

target_include_directories(Tests PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(Tests
    PRIVATE
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(Tests)
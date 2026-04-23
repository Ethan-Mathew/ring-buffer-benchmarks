add_executable(StressTests
    MutexBufferStressTest.cpp
    SPSCBufferStressTest.cpp
)

target_include_directories(StressTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(StressTests
    PRIVATE
        GTest::gtest_main
) 

include(GoogleTest)
gtest_discover_tests(StressTests)
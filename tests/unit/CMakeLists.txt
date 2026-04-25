add_executable(UnitTests
    MutexBufferTest.cpp
    SPSCBufferTest.cpp
)

target_include_directories(UnitTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(UnitTests
    PRIVATE
        GTest::gtest_main
) 

include(GoogleTest)
gtest_discover_tests(UnitTests)
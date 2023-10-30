cmake_minimum_required(VERSION 3.19)
project(quash)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(thirdparty/fuzztest)

enable_testing()

include(GoogleTest)

fuzztest_setup_fuzzing_flags()

add_executable(
    fuzz_test
    fuzz_test.cpp
)

link_fuzztest(fuzz_test)
gtest_discover_tests(fuzz_test)

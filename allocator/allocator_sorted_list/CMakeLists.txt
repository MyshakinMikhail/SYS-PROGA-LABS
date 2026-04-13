cmake_minimum_required(VERSION 3.14)
project(allocator_sorted_list)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

add_subdirectory(tests)

add_library(
        sys_prog_allctr_allctr_srtd_lst
        src/allocator_sorted_list.cpp)

target_include_directories(
        sys_prog_allctr_allctr_srtd_lst
        PUBLIC
        ./include)

target_link_libraries(
        sys_prog_allctr_allctr_srtd_lst
        PUBLIC
        sys_prog_cmmn
        sys_prog_allctr_allctr)
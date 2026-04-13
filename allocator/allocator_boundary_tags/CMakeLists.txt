cmake_minimum_required(VERSION 3.14)
project(allocator_boundary_tags)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Подключаем Google Test (если еще не подключен в корневом проекте)
# Но если корневой проект уже подключает googletest, эту часть можно пропустить
if(NOT TARGET gtest)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Подключаем тесты
add_subdirectory(tests)

# Создаем библиотеку аллокатора
add_library(
        sys_prog_allctr_allctr_bndr_tgs
        src/allocator_boundary_tags.cpp)

target_include_directories(
        sys_prog_allctr_allctr_bndr_tgs
        PUBLIC
        ./include)

target_link_libraries(
        sys_prog_allctr_allctr_bndr_tgs
        PUBLIC
        sys_prog_cmmn
        sys_prog_allctr_allctr)
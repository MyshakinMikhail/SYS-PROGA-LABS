add_executable(
        sys_prog_allctr_allctr_bndr_tgs_tests
        allocator_boundary_tags_tests.cpp)

target_link_libraries(
        sys_prog_allctr_allctr_bndr_tgs_tests
        PRIVATE
        gtest_main
        sys_prog_allctr_allctr_bndr_tgs)

target_include_directories(
        sys_prog_allctr_allctr_bndr_tgs_tests
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_SOURCE_DIR}/common/include)

# Автоматическое обнаружение и регистрация тестов
include(GoogleTest)
gtest_discover_tests(sys_prog_allctr_allctr_bndr_tgs_tests)
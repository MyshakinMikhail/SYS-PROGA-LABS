add_executable(
        sys_prog_allctr_allctr_srtd_lst_tests
        allocator_sorted_list_tests.cpp)

target_link_libraries(
        sys_prog_allctr_allctr_srtd_lst_tests
        PRIVATE
        gtest_main
        sys_prog_allctr_allctr_srtd_lst)

target_include_directories(
        sys_prog_allctr_allctr_srtd_lst_tests
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_SOURCE_DIR}/common/include)

include(GoogleTest)
gtest_discover_tests(sys_prog_allctr_allctr_srtd_lst_tests)
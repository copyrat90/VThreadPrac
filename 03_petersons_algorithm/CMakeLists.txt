if(MSVC)
    add_executable(03_petersons_algorithm main.cpp)
    target_compile_options(03_petersons_algorithm PRIVATE ${vtp_compile_options})
    target_link_libraries(03_petersons_algorithm Synchronization)

    add_test(NAME test_petersons_algorithm COMMAND 03_petersons_algorithm)
endif()

add_executable(03_petersons_algorithm_cxxstd main_cxxstd.cpp)
target_compile_options(03_petersons_algorithm_cxxstd PRIVATE ${vtp_compile_options})
target_link_libraries(03_petersons_algorithm_cxxstd PRIVATE Threads::Threads)

add_executable(02_mutex main.cpp)
target_compile_options(02_mutex PRIVATE ${vtp_compile_options})
target_link_libraries(02_mutex PRIVATE Threads::Threads)
if(MSVC)
    target_link_libraries(02_mutex PRIVATE Synchronization)
endif()

add_test(NAME test_mutex COMMAND 02_mutex)

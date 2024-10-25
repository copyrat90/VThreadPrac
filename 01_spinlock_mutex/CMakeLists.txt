add_executable(01_spinlock_mutex main.cpp)
target_compile_options(01_spinlock_mutex PRIVATE ${vtp_compile_options})
target_link_libraries(01_spinlock_mutex PRIVATE Threads::Threads)

add_test(NAME test_spinlock_mutex COMMAND 01_spinlock_mutex)

add_executable(08_stack_issue_1 stack_issue_1.cpp)
target_compile_options(08_stack_issue_1 PRIVATE ${vtp_compile_options})
target_link_libraries(08_stack_issue_1 PRIVATE Threads::Threads)

add_executable(08_stack_issue_2 stack_issue_2.cpp)
target_compile_options(08_stack_issue_2 PRIVATE ${vtp_compile_options})
target_link_libraries(08_stack_issue_2 PRIVATE Threads::Threads)


include(FetchContent)
FetchContent_Declare(NetBuff
    GIT_REPOSITORY https://github.com/copyrat90/NetBuff.git
    GIT_TAG main
)
FetchContent_MakeAvailable(NetBuff)

add_executable(08_stack_issue_1 stack_issue_1.cpp)
target_compile_options(08_stack_issue_1 PRIVATE ${vtp_compile_options})
target_link_libraries(08_stack_issue_1 PRIVATE Threads::Threads)

add_executable(08_stack_issue_2 stack_issue_2.cpp)
target_compile_options(08_stack_issue_2 PRIVATE ${vtp_compile_options})
target_link_libraries(08_stack_issue_2 PRIVATE Threads::Threads)

add_executable(08_queue_issue queue_issue.cpp)
target_compile_options(08_queue_issue PRIVATE ${vtp_compile_options})
target_link_libraries(08_queue_issue PRIVATE NetBuff Threads::Threads)

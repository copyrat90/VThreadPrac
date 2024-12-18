
include(FetchContent)
FetchContent_Declare(NetBuff
    GIT_REPOSITORY https://github.com/copyrat90/NetBuff.git
    GIT_TAG main
)
FetchContent_MakeAvailable(NetBuff)

set(GCC_SANITIZER_AVAILABLE FALSE)
if(NOT WIN32)
    if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        set(GCC_SANITIZER_AVAILABLE TRUE)
    endif()
endif()

add_executable(08_stack_issue_1 stack_issue_1.cpp)
target_compile_options(08_stack_issue_1 PRIVATE ${vtp_compile_options})
target_link_libraries(08_stack_issue_1 PRIVATE Threads::Threads)

add_executable(08_stack_issue_2 stack_issue_2.cpp)
target_compile_options(08_stack_issue_2 PRIVATE ${vtp_compile_options})
target_link_libraries(08_stack_issue_2 PRIVATE Threads::Threads)

add_executable(08_queue_issue_1 queue_issue_1.cpp)
target_compile_options(08_queue_issue_1 PRIVATE ${vtp_compile_options})
target_link_libraries(08_queue_issue_1 PRIVATE NetBuff Threads::Threads)

add_executable(08_queue_validate_automatic queue_validate_automatic.cpp)
target_compile_options(08_queue_validate_automatic PRIVATE ${vtp_compile_options})
target_link_libraries(08_queue_validate_automatic PRIVATE NetBuff Threads::Threads)

if(GCC_SANITIZER_AVAILABLE)
    target_compile_options(08_queue_validate_automatic PRIVATE -fsanitize=address)
    target_link_options(08_queue_validate_automatic PRIVATE -fsanitize=address)
elseif(MSVC AND NB_TEST_MSVC_SANITIZER)
    target_compile_options(08_queue_validate_automatic PRIVATE /fsanitize=address)
    target_link_options(08_queue_validate_automatic PRIVATE /INCREMENTAL:NO /DEBUG)
endif()

include(CTest)
enable_testing()
add_test(NAME test_queue_validate_automatic COMMAND 08_queue_validate_automatic)

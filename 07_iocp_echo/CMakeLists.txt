include(FetchContent)
FetchContent_Declare(DirtySocks
    GIT_REPOSITORY https://github.com/copyrat90/DirtySocks.git
    GIT_TAG main
)
FetchContent_MakeAvailable(DirtySocks)

add_executable(07_iocp_echo_client client.cpp)
target_compile_options(07_iocp_echo_client PRIVATE ${vtp_compile_options})
target_link_libraries(07_iocp_echo_client PRIVATE DirtySocks)

if(MSVC)
    FetchContent_Declare(NetBuff
        GIT_REPOSITORY https://github.com/copyrat90/NetBuff.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(NetBuff)

    add_executable(07_iocp_echo_server server.cpp)
    target_compile_options(07_iocp_echo_server PRIVATE ${vtp_compile_options})
    target_link_libraries(07_iocp_echo_server PRIVATE DirtySocks NetBuff)
endif()

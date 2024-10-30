
include(FetchContent)
FetchContent_Declare(NetBuff
    GIT_REPOSITORY https://github.com/copyrat90/NetBuff.git
    GIT_TAG main
)
FetchContent_MakeAvailable(NetBuff)

if(MSVC)
    add_executable(06_ring_buffer_job_worker main.cpp)
    target_compile_options(06_ring_buffer_job_worker PRIVATE ${vtp_compile_options})
    target_link_libraries(06_ring_buffer_job_worker PRIVATE winmm NetBuff)
endif()

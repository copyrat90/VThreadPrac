#include "common.hpp"

#include <DirtySocks/System.hpp>
#include <DirtySocks/TcpListener.hpp>
#include <DirtySocks/TcpSocket.hpp>
#include <NetBuff/SpscRingByteBuffer.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <utility>

#include <Windows.h>
#include <process.h>

static constexpr std::uint16_t PORT = 32983;

static constexpr std::size_t RING_BUF_SIZE = 2048;

struct Session
{
    using Id = int;

    Id id;
    ds::TcpSocket sock;
    nb::SpscRingByteBuffer<> buf;
    WSAOVERLAPPED send_io;
    WSAOVERLAPPED recv_io;

    Session(Session::Id id_, ds::TcpSocket&& sock_)
        : id(id_), sock(std::move(sock_)), buf(RING_BUF_SIZE), send_io{}, recv_io{}
    {
    }
};

struct SessionMap
{
    std::unordered_map<Session::Id, Session> map;
    SRWLOCK lock;

    SessionMap()
    {
        InitializeSRWLock(&lock);
    }
};

SessionMap g_sessions;

HANDLE g_iocp;

unsigned __stdcall worker(void* arg)
{
    HANDLE iocp = (HANDLE)arg;

    std::error_code ec;

    while (true)
    {
        // wait for async io
        DWORD transferred = 0;
        Session* session = nullptr;
        OVERLAPPED* overlapped = nullptr;
        const bool success = GetQueuedCompletionStatus(iocp, &transferred, (PULONG_PTR)&session, &overlapped, INFINITE);
        // disconnect session
        if (!success || !overlapped || 0 == transferred)
        {
            if (overlapped)
            {
                AcquireSRWLockExclusive(&g_sessions.lock);
                const auto erased = g_sessions.map.erase(session->id);
                ReleaseSRWLockExclusive(&g_sessions.lock);
                assert(erased);
            }
            continue;
        }

        // TODO
    }

    return 0;
}

int main()
{
    std::error_code ec;

    ds::System::init(ec);
    check_ec(ec);

    const unsigned cores = std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4;

    // prepare iocp
    g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (nullptr == g_iocp)
        check_ec(ds::System::get_last_error_code());

    for (unsigned i = 0; i < cores * 2; ++i)
    {
        auto worker_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, worker, g_iocp, 0, nullptr));
        if (!worker_thread)
        {
            std::cout << "worker thread #" << i << " creation failed" << std::endl;
            return -1;
        }
        CloseHandle(worker_thread);
    }

    // prepare listener socket
    ds::TcpListener listener;
    listener.listen(ds::SocketAddress::any(PORT, ds::IpVersion::V4), ec);
    check_ec(ec);

    // reuse addr
    const int enable = true;
    if (SOCKET_ERROR ==
        setsockopt(listener.get_handle(), SOL_SOCKET, SO_REUSEADDR, (const char*)&enable, sizeof(enable)))
        check_ec(ds::System::get_last_error_code());

    Session::Id next_session_id = 0;

    // accept new session
    while (true)
    {
        ds::TcpSocket sock;
        listener.accept(sock, ec);
        check_ec(ec);

        sock.set_non_blocking(true, ec);
        check_ec(ec);

        // no nagle
        if (SOCKET_ERROR ==
            setsockopt(sock.get_handle(), IPPROTO_TCP, TCP_NODELAY, (const char*)&enable, sizeof(enable)))
            check_ec(ec);

        // direct i/o
        const int zero = 0;
        if (SOCKET_ERROR == setsockopt(sock.get_handle(), SOL_SOCKET, SO_SNDBUF, (const char*)&zero, sizeof(zero)))
            check_ec(ds::System::get_last_error_code());

        AcquireSRWLockExclusive(&g_sessions.lock);
        auto [it, inserted] = g_sessions.map.try_emplace(next_session_id, next_session_id, std::move(sock));
        ReleaseSRWLockExclusive(&g_sessions.lock);
        assert(inserted);
        ++next_session_id;

        auto& session = it->second;

        // associate new session with iocp
        auto iocp = CreateIoCompletionPort((HANDLE)session.sock.get_handle(), g_iocp, (ULONG_PTR)&session, 0);
        if (nullptr == iocp)
            check_ec(ds::System::get_last_error_code());

        // async receive request
        ds::IoBuffer io_buf[1];
        io_buf[0].buf = reinterpret_cast<char*>(session.buf.data() + session.buf.write_pos());
        io_buf[0].len = static_cast<unsigned>(session.buf.available_write());
        session.sock.receive(io_buf, session.recv_io, ec);
    }

    CloseHandle(g_iocp);

    AcquireSRWLockExclusive(&g_sessions.lock);
    g_sessions.map.clear();
    ReleaseSRWLockExclusive(&g_sessions.lock);

    ds::System::destroy();
}

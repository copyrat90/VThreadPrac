#include "common.hpp"

#include <DirtySocks/System.hpp>
#include <DirtySocks/TcpSocket.hpp>

#include <cstdint>
#include <iostream>
#include <string>

static constexpr std::uint16_t PORT = 32983;
static constexpr const char* PORT_STR = "32983";

int main(int argc, char** argv)
{
    if (argc != 1 && argc != 2)
    {
        std::cout << "Usage: 07_iocp_echo_client <IPv4 address>" << std::endl;
        return 1;
    }

    std::error_code ec;

    ds::System::init(ec);
    check_ec(ec);

    auto addr = ds::SocketAddress::resolve(argc == 2 ? argv[1] : "localhost", PORT_STR, ds::IpVersion::V4, ec);
    if (!addr)
        check_ec(ec);

    ds::TcpSocket sock;
    sock.connect(*addr, ec);
    check_ec(ec);

    std::string input;
    std::string echoed;

    std::cout << "Input: ";
    while (std::getline(std::cin, input))
    {
        for (std::size_t pos = 0; pos < input.size();)
        {
            std::size_t sent;
            sock.send(input.data() + pos, input.size() - pos, sent, ec);
            check_ec(ec);
            pos += sent;
        }

        echoed.resize(input.size());
        for (std::size_t pos = 0; pos < echoed.size();)
        {
            std::size_t received;
            sock.receive(echoed.data() + pos, echoed.size() - pos, received, ec);
            check_ec(ec);
            pos += received;
        }

        std::cout << "Echo: " << echoed << "\nInput: ";
    }

    sock.close();

    ds::System::destroy();
}

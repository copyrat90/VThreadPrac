#pragma once

#include <cstdlib>
#include <iostream>
#include <source_location>
#include <system_error>

inline void check_ec(std::error_code ec, std::source_location loc = std::source_location::current())
{
    if (ec)
    {
        std::cout << "Error in " << loc.file_name() << ":" << loc.line() << " (" << loc.function_name() << ")\n";
        std::cout << "\t[" << ec << "] " << ec.message() << std::endl;
        std::exit(1);
    }
}

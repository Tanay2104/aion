export module aion.core:common;

import std;
export namespace aion::core
{
    // ansi colors.
    constexpr std::string RED = "\033[1;31m";
    constexpr std::string YELLOW = "\033[1;33m";
    constexpr std::string BLUE = "\033[1;34m";
    constexpr std::string GREEN = "\033[1;32m";
    constexpr std::string RESET = "\033[0m";
    constexpr std::string BOLD = "\033[1m";


    enum class Type : std::uint8_t
    {
        INT,
        CHAR,
        FLOAT,
        BOOL,
    };


    // Note: Currently we are using only uint64 to represent the states. This implies
    // a maximum of 64 states. Adding AVX2 will increase this to 128 states, or 256
    // if we solve the lane crossing problems correctly. Further inclusion of multiple
    // registers and a virtual register interface, perhaps implemented with intelligent
    // graph partitioning, allows this number to increase even more. For many applications,
    // even 64 can be sufficient.
    constexpr std::uint16_t MAX_STATES = 63;
}
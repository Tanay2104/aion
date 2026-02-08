//
// Created by tanay on 2/1/26.
//

module aion.core;

namespace aion::core
{
    void Diagnostics::set_source_location(const std::string_view src)
    {
        current_source_code = src;
    }
    void Diagnostics::print_snippet(const SourceLocation& location) const
    {
        std::size_t line_start = 0;
        std::size_t current_line = 1;

        for (std::size_t i = 0; i < current_source_code.length(); ++i)
        {
            if (current_line == location.line)
            {
                line_start = i;
                break;
            }
            if (current_source_code[i] == '\n')
            {
                ++current_line;
            }
        }
        std::size_t line_end = current_source_code.find('\n', line_start);
        if (line_end == std::string_view::npos) line_end = current_source_code.length();

        std::string_view line_text = current_source_code.substr(line_start, line_end - line_start);

        // printing the line.
        std::cerr << std::format("{} {:4} | {} {}", BLUE, location.line, RESET, line_text);
        // print the caret.
        std::string padding(location.column+6, ' ');
        std::cerr << std::format("{}^{}\n", BOLD, RESET);
    }

    void Diagnostics::report_error(const SourceLocation& location, std::string_view msg)
    {
        ++error_count;
        std::cerr << std::format("{}[ERROR]{} {}:{} {}\n", aion::core::RED, aion::core::RESET, location.line, location.column, msg);
        print_snippet(location);
    }
    void Diagnostics::report_warning(const SourceLocation& location, std::string_view msg)
    {
        ++warning_count;
        std::cerr << std::format("{} [WARNING] {}:{}: {}\n", aion::core::YELLOW, aion::core::RESET, location.line, location.column, msg);
        print_snippet(location);
    }
    void Diagnostics::report_internal_error(std::string_view msg)
    {
        // internal compiler error. Must exit.
        std::cerr << std::format("{} [ERROR]{} Internal Compiler Error: {}\n", aion::core::RED, aion::core::RESET, msg);
        std::exit(1);
    }
    [[nodiscard]] bool Diagnostics::has_errors() const
    {
        return (error_count > 0);
    }
    [[nodiscard]] bool Diagnostics::has_warnings() const
    {
        return (warning_count > 0);
    }

    [[nodiscard]] std::uint8_t Diagnostics::get_error_count() const
    {
        return error_count;
    }
    [[nodiscard]] std::uint8_t Diagnostics::get_warning_count() const
    {
        return warning_count;
    }
};
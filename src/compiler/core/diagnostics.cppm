/* diagnostics.cppm
 * A centralised way to report now.
 * Keeping it simple right now
 */
export module aion.core:diagnostics;
import std;
import :common;

export namespace aion {
  namespace core {
    constexpr std::uint8_t MAX_ERROR_COUNT = 16;
    constexpr std::uint8_t MAX_WARNING_COUNT = 32;
    struct SourceLocation {
      std::size_t line = 1;
      std::size_t column = 1;
      SourceLocation(const std::size_t line, const std::size_t column)
    : line(line), column(column) {}
    };
    class Diagnostics
    {
    private:
      std::uint8_t error_count = 0;
      std::uint8_t warning_count = 0;
      std::string_view current_source_code{}; // Reference to full file content.


      // helper to print code line.
      void print_snippet(const SourceLocation &location) const;
    public:
      void set_source_location(const std::string_view src);
      // reporting stuff to user.
      void report_error(const SourceLocation &location, std::string_view msg);
      void report_warning(const SourceLocation &location, std::string_view msg);

      // report internal error.
      static void report_internal_error(std::string_view msg);

      [[nodiscard]] bool has_errors() const;
      [[nodiscard]] bool has_warnings() const;

      [[nodiscard]] std::uint8_t get_error_count() const;
      [[nodiscard]] std::uint8_t get_warning_count() const;
    };
  };
};
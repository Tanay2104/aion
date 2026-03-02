/* config.cppm
 * Defines compilation targets such as target arch or compiler config.
 * As of now, only 1 option is there
 */
export module aion.core:config;

import :diagnostics;
import :common;
import std;

export namespace aion {
namespace core {
using namespace aion;

enum class Arch : std::uint8_t {SCALAR64, AVX2, AVX512, SSE42, NEON };
enum class OptimLevel : std::uint8_t {
  NONE,
  FIRST_LEVEL,
  SECOND_LEVEL,
  THIRD_LEVEL,
};

enum class IR : std::uint8_t { NONE, TOKENS, AST, NFA };

enum class Verbosity : std::uint8_t {
  NONE = 0,
  FIRST_LEVEL = 1,
  SECOND_LEVEL = 2,
  THIRD_LEVEL = 3,
};

  constexpr std::string DEFAULT_NAME ="aion_out";

struct Options {
  Arch arch = Arch::SCALAR64;
  OptimLevel optim_level = OptimLevel::NONE;
  IR ir = IR::NONE;
  Verbosity verbosity = Verbosity::FIRST_LEVEL;
  std::string output_filename = DEFAULT_NAME;
};

struct CompilationContext {
  // User-provided options
  core::Options options;

  // Session-wide objects
  core::Diagnostics diagnostics{};

  // Helper for verbosity
  void log(int level, std::string_view msg, std::string_view color = aion::core::GREEN) const {
    if (static_cast<int>(options.verbosity) >= level) {
      std::println("{} [INFO] {} {}", color, aion::core::RESET, msg);
    }
  }
};
}; // namespace core
} // namespace aion

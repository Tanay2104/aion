/* config.cppm
 * Defines compilation targets such as target arch or compiler config.
 * As of now, only 1 option is there
 */
export module aion.core:config;

import :diagnostics;
import :symbol_table;
import std;

export namespace aion {
namespace core {
using namespace aion;

enum class Arch : std::uint8_t {
  AVX2,
  AVX512,
  SSE42,
  NEON
};
enum class OptimLevel : std::uint8_t {
  NONE,
  FIRST_LEVEL,
  SECOND_LEVEL,
  THIRD_LEVEL,
};

enum class IR : std::uint8_t {
  NONE,
  TOKENS,
  AST,
  NFA
};

enum class Verbosity : std::uint8_t {
  NONE,
  FIRST_LEVEL,
  SECOND_LEVEL,
  THIRD_LEVEL,
};

struct Options {
  Arch arch = Arch::AVX2;
  OptimLevel optim_level = OptimLevel::NONE;
  IR ir = IR::NONE;
  Verbosity verbosity = Verbosity::FIRST_LEVEL;
  std::string output_filename = "";
};

struct CompilationContext {
  // User-provided options
  core::Options options;

  // Session-wide objects
  core::Diagnostics diagnostics{};
  core::SymbolTable symbols{};

  // Helper for verbosity
  void log(int level, std::string_view msg) const {
    if (static_cast<int>(options.verbosity) >= level) {
      std::println("[INFO] {}", msg);
    }
  }
};
};
}
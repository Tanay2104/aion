/* Utility for parsing command line arguements
 */
export module aion.utils:argparser;
import std;
import aion.core;

std::string help_string = "Aion is a symbolic regex compiler designed for Complex Event Processing with low jitter.\n"
                          "It accepts a regex file, to be compiled into C++ code\n"
                          "Basic usage: ./aionc <input_filename> [options]\n"
                          "Allowed options: \n"
                          "\t -o <output_filename>\n"
                          "\t\t Specify the location of the generated cppm file which is to be imported by user\n"
                          "\t -march [native, AVX2, AVX512, SSE, NEON]\n"
                          "\t\t Specify the architecture and SIMD extensions available on the CPU\n"
                          "\t\t NOTE: Only AVX2 is being developed as of 30 Jan, 2026\n"
                          "\t -O [O0, O1, O2, O3]\n"
                          "\t\t Control the level of optimisation the aion compiler performs.\n"
                          "\t\t NOTE: Only O0 is being developed as of 30 Jan, 2026\n"
                          "\t -v, --version\n"
                          "\t\t Displays current version\n"
                          "\t -V, --verbose [V0, V1, V2, V3]\n"
                          "\t\t Prints detailed information about each compilation stage. Select verbosity by -V\n"
                          "\t\t Default: -V1. --verbose results in -V2 verbosity\n"
                          "\t -e, --emit [TOKENS, AST, NFA]\n"
                          "\t\t Dump intermediate TOKENS (dump the lexer output), AST (pretty-print the AST), NFA (in DOT format)\n"
                          "\t -nj, --nojitter\n"
                         "\t\t Codegen produces state machine which has no jitter and O(n^2) complexity but may be slower on average\n"
                          "For more details, please see examples and docs.\n";

std::string current_version = "0.1.0\n";

// Source - https://stackoverflow.com/a
// Posted by iain, modified by community. See post 'Timeline' for change history
// Retrieved 2025-12-23, License - CC BY-SA 3.0
class ArgParser {
public:
  ArgParser(int &argc, char **argv) {
    for (int i = 1; i < argc; ++i)
      this->tokens.push_back(std::string(argv[i]));
  }
  /// @author iain
  const std::string &getCmdOption(const std::string &option) const {
    std::vector<std::string>::const_iterator itr;
    itr = std::find(this->tokens.begin(), this->tokens.end(), option);
    if (itr != this->tokens.end() && ++itr != this->tokens.end()) {
      return *itr;
    }
    static const std::string empty_string("");
    return empty_string;
  }
  /// @author iain
  bool cmdOptionExists(const std::string &option) const {
    return std::find(this->tokens.begin(), this->tokens.end(), option) !=
           this->tokens.end();
  }
private:
  std::vector<std::string> tokens;
};

export namespace aion {
namespace utils {
  core::Options argparse(int &argc, char **argv) {
    const std::string filename = argv[1];
    const ArgParser input(argc, argv);
    core::Options options;

    if (input.cmdOptionExists("-h") || input.cmdOptionExists("--help")) {
      std::print("{}", help_string);
      std::exit(0);
    }
    if (input.cmdOptionExists("-v") || input.cmdOptionExists("--version")) {
      std::print("{}", current_version);
      std::exit(0);
    }
    if (input.cmdOptionExists("-o")) {
      options.output_filename = input.getCmdOption("-o");
      if (options.output_filename == "") {
        std::println("Invalid argument");
        std::exit(1);
      }
    }
    if (input.cmdOptionExists("-nj") || input.cmdOptionExists("--nojitter"))
    {
      options.jitter = false;
    }

    if (input.cmdOptionExists("-e") || input.cmdOptionExists("--emit")) {
      std::string ir = input.getCmdOption("-e");
      if (ir.empty()) {
        ir = input.getCmdOption("--emit");
      }
      if (ir.empty()) {
        std::println("Invalid argument");
        std::exit(1);
      }
      if (ir ==  "TOKENS") {
        options.ir = core::IR::TOKENS;
      }
      else if (ir ==  "AST") {
        options.ir = core::IR::AST;
      }
      else if (ir == "NFA") {
        options.ir = core::IR::NFA;
      }
      else {
        options.ir = core::IR::NONE;
      }
    }

    if (input.cmdOptionExists("--verbose") || input.cmdOptionExists("-V2")) {
      options.verbosity = core::Verbosity::SECOND_LEVEL;
    }
    else if (input.cmdOptionExists("-V0")) {
      options.verbosity = core::Verbosity::NONE;
    }
    else if (input.cmdOptionExists("-V1")) {
      options.verbosity = core::Verbosity::FIRST_LEVEL;
    }
    else if (input.cmdOptionExists("-V3")) {
      options.verbosity = core::Verbosity::THIRD_LEVEL;
    }

    // Guys please don't put anything else now.
    if (input.cmdOptionExists("-O0")) {
      options.optim_level = core::OptimLevel::NONE;
    }
    else if (input.cmdOptionExists("-O1")) {
      // To be deleted later.
      std::println("Only -O0 implemented now");
      std::exit(1);
      options.optim_level = core::OptimLevel::FIRST_LEVEL;
    }
    else if (input.cmdOptionExists("-O2")) {
      // To be deleted later.
      std::println("Only -O0 implemented now");
      std::exit(1);
      options.optim_level = core::OptimLevel::SECOND_LEVEL;
    }
    else if (input.cmdOptionExists("-O3")) {
      // To be deleted later.
      std::println("Only -O0 implemented now");
      std::exit(1);
      options.optim_level = core::OptimLevel::THIRD_LEVEL;
    }
    return options;
  }
};
};
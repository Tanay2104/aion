/* codegen.cppm
 * Exports the emitter
 */
export module aion.codegen;

import std;

export import :cemitter;
export import :eventgen;
export import :predicategen;
export import :regexgen;

export namespace aion::codegen
{
    constexpr std::string event_name = "event";
    constexpr std::string function_prefix = "_evaluate_predicate_";
    constexpr std::string function_suffix = "_(const Event& event)";
    constexpr std::string function_call_suffix = "_("+event_name+")";
}
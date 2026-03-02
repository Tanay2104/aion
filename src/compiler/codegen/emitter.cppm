/* emitter.cppm
 * Contains code for a general C code emitter.
 */
export module aion.codegen:cemitter;

import std;
import aion.frontend;

namespace  aion::codegen
{
    export class CEmitter
    {
    private:
        std::string out{};
        std::uint16_t indent_level = 0;
    public:
        void emit_line(std::string_view line);
        void indent();
        void dedent();
        void emit_block_start(std::string_view line);
        void emit_block_end(std::string_view suffix = "");
        void dump(std::string_view filename) const;
    };

    export void emit_headers(CEmitter& emitter);
    export void emit_footers(CEmitter& emitter);


}
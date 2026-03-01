/* glushkov.cpp
 * Implements glushkov.cppm
 */

module aion.automata;

import std;
import aion.frontend;

namespace aion::automata
{
    std::unordered_map<std::string_view, NFA_64> convert_to_nfa_64(const frontend::AionFile& ast, const frontend::SymbolTable& table, const core::CompilationContext& ctxt)
    {
        std::unordered_map<std::string_view, NFA_64> all_nfa;
        for (const frontend::RegexDecl& regex_decl : ast.regexes)
        {
            ctxt.log(3, std::format("Making NFA for regex named {}", regex_decl.name));
            GlushkovVisitor visitor(std::get<frontend::RegexMetadata>(table.resolve(regex_decl.name)->details), ctxt);
            regex_decl.expr->accept(visitor);
            // The NFA generation is complete.
            all_nfa[regex_decl.name] = visitor.convert_to_NFA_64();
        }
        return all_nfa;
    }


    GlushkovVisitor::GlushkovVisitor(const frontend::RegexMetadata& _meta, const core::CompilationContext& _ctxt)
        : meta(_meta), ctxt(_ctxt)
    {

    }

    void GlushkovVisitor::add_character(const frontend::RegexExpr* node)
    {
        GlushkovFragment fragment;
        fragment.nullable = false;
        fragment.first.insert(meta.node_to_pos_ids.at(node));
        fragment.last.insert(meta.node_to_pos_ids.at(node));

        stack.push(fragment);
    }

    void GlushkovVisitor::visit(const frontend::RegexPrimary& regex_primary)
    {
        std::visit([this, &regex_primary](const auto& arg) {

            // get the raw type of the argument.
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::string_view>) {
                // Once again, a new character.
                add_character(&regex_primary);
            }
            else if constexpr (std::is_same_v<T, frontend::RegexWildcard>) {
                arg.accept(*this);
            }
            else if constexpr (std::is_same_v<T, std::unique_ptr<frontend::RegexExpr>>) {
                arg->accept(*this);
            }

        }, regex_primary.expr);
    }

    void GlushkovVisitor::visit(const frontend::RegexUnion& node)
    {
        const std::uint16_t num_unions =  static_cast<std::uint16_t>(node.options.size());
        for (const auto& alt: node.options)
        {
            alt->accept(*this);
        }
        // Now that we have pushed num_union glushkov fragments onto stack,
        // we will pop them and make the bigger fragment.
        GlushkovFragment new_fragment;

        std::vector<GlushkovFragment> fragments;
        fragments.reserve(num_unions);
        for (std::uint16_t i = 0; i < num_unions; ++i)
        {
            fragments.push_back(stack.top());
            stack.pop();
        }

        // Applying the Glushkov construction rules.
        new_fragment.nullable = false;
        for (const GlushkovFragment& fragment : fragments)
        {
            if (fragment.nullable)
            {
                new_fragment.nullable = true;
            }
            new_fragment.first.insert(fragment.first.begin(), fragment.first.end());
            new_fragment.last.insert(fragment.last.begin(), fragment.last.end());
        }

        stack.push(new_fragment);
        // Union by itself doesn't change follow.
    }



    void GlushkovVisitor::visit(const frontend::RegexConcat& node)
    {
        const std::uint16_t num_concats = static_cast<std::uint16_t>(node.sequence.size());
        for (const auto& seqpart: node.sequence)
        {
            seqpart->accept(*this);
        }

        // Now the top of te stack will contain the last pattern,
        // and num_concats below will contain the first pattern.

        std::vector<GlushkovFragment> fragments(num_concats);
        for (std::uint16_t i = 0; i < num_concats; ++i)
        {
            fragments[num_concats - i - 1] = stack.top();
            stack.pop();
        }
        // Now our vector contains the sequences in the order they appear.
        GlushkovFragment new_fragment;

        // We do a sweep over all fragments such that new fragment is nullable
        // if all fragments are nullable.
        new_fragment.nullable = true;
        for (const GlushkovFragment& fragment : fragments)
        {
            new_fragment.nullable = (fragment.nullable && new_fragment.nullable);
        }

        // Computing the first and last sets is a lot trickier though.
        // The first of first frag will be in new frag first (obviously), and similarly for last.
        // But if the first i are nullable then the first i+1 can be inserted into new first.
        // Similarly, if last j are nullable then last of j+1 can be inserted into our new last.

        new_fragment.first.insert(fragments.begin()->first.begin(), fragments.begin()->first.end());
        new_fragment.last.insert(fragments.back().last.begin(), fragments.back().last.end());

        for (std::uint16_t i = 0; i < num_concats-1; ++i)
        {
            if (fragments[i].nullable)
            {
                new_fragment.first.insert(fragments[i+1].first.begin(), fragments[i+1].first.end());
            }
            else
            {
                break;
            }
        } // Note that we rely on the fact that all from 0 to i must be nullable for i+1 to insert.

        for (std::uint16_t j = num_concats; j > 0; --j)
        {
            if (fragments[j-1].nullable)
            {
                new_fragment.last.insert(fragments[j].last.begin(), fragments[j].last.end());
            }
            else
            {
                break;
            }
        }

        // Now we need to update the follow sets.
        for (std::uint16_t i = 0; i < num_concats-1; ++i)
        {
            for (std::uint16_t end : fragments[i].last)
            {
                follow[end].insert(fragments[i+1].first.begin(), fragments[i+1].first.end());
            }
        }

        stack.push(new_fragment);
    }

    void GlushkovVisitor::visit(const frontend::RegexWildcard& node)
    {
        // This is also basically a new letter.
        add_character(&node);
    }

    void GlushkovVisitor::visit(const frontend::RegexStar& node)
    {
        node.inner->accept(*this);

        GlushkovFragment fragment = stack.top();
        stack.pop();

        GlushkovFragment new_fragment;
        new_fragment.nullable = true; // Kleene Star is always nullable.

        new_fragment.first.insert(fragment.first.begin(), fragment.first.end());
        new_fragment.last.insert(fragment.last.begin(), fragment.last.end());

        // Kleene star properties.
        for (std::uint16_t last : new_fragment.last)
        {
            for (std::uint16_t first : new_fragment.first)
            {
                follow[last].insert(first);
            }
        }

        stack.push(new_fragment);
    }

    void GlushkovVisitor::visit(const frontend::RegexRefExpr& node)
    {
        // This is basically a new letter.
        add_character(&node);
    }

    NFA_64 GlushkovVisitor::convert_to_NFA_64()
    {
        if (stack.size() != 1)
        {
            ctxt.diagnostics.report_internal_error("Multiple glushkov fragments remaining.");
        }

        GlushkovFragment fragment = stack.top();
        stack.pop();

        NFA_64 nfa_64;

        nfa_64.nullable = fragment.nullable;

        nfa_64.first = 0ULL;
        for (const std::uint16_t first : fragment.first)
        {
            nfa_64.first |= (1ULL << first);
        }

        nfa_64.last = 0ULL;
        for (const std::uint16_t last : fragment.last)
        {
            nfa_64.last |= (1ULL << last);
        }

        for (auto& [start, end_set] : follow)
        {
            nfa_64.follow[start] = 0ULL;
            for (const std::uint16_t end: end_set)
            {
                nfa_64.follow[start] |= (1ULL << end);
            }
        }

        return nfa_64;
    }
};
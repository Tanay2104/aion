# --- 1. CORE (Level 0) ---
add_library(aion_core STATIC)
target_sources(aion_core PUBLIC
    FILE_SET CXX_MODULES
        FILES
            core/core.cppm
            core/config.cppm
            core/diagnostics.cppm
            core/common.cppm
        PRIVATE
            core/diagnostics.cpp

)
# --- 2. FRONTEND (Level 1) ---
add_library(aion_frontend STATIC)
target_sources(aion_frontend PUBLIC
    FILE_SET CXX_MODULES
        FILES
            frontend/frontend.cppm
            frontend/ast.cppm
            frontend/lexer.cppm
            frontend/parser.cppm
            frontend/symbol_table.cppm
        PRIVATE
            frontend/lexer.cpp
            frontend/parser.cpp
            frontend/parser_event.cpp
            frontend/parser_pred.cpp
            frontend/parser_regex.cpp
            frontend/ast_dump.cpp
            frontend/symbol_table.cpp
)
# Frontend depends on Core types
target_link_libraries(aion_frontend PUBLIC aion_core aion_utils)

# --- 3. ANALYSIS (Level 2) ---
add_library(aion_analysis STATIC)
target_sources(aion_analysis PUBLIC
    FILE_SET CXX_MODULES
        FILES
            analysis/analysis.cppm
            analysis/minimizer.cppm
            analysis/alphabet.cppm
        PRIVATE
            analysis/alphabet.cpp
)
# Analysis depends on Frontend (AST)
target_link_libraries(aion_analysis PUBLIC aion_frontend)

# --- 4. AUTOMATA (Level 3) ---
add_library(aion_automata STATIC)
target_sources(aion_automata PUBLIC
    FILE_SET CXX_MODULES
        FILES
            automata/automata.cppm
            automata/nfa.cppm
            automata/glushkov.cppm
            automata/partitioner.cppm
        PRIVATE
            automata/glushkov.cpp
)
# Automata uses Analysis results
target_link_libraries(aion_automata PUBLIC aion_analysis)

# --- 5. CODEGEN (Level 4) ---
add_library(aion_codegen STATIC)
target_sources(aion_codegen PUBLIC
    FILE_SET CXX_MODULES
        FILES
            codegen/codegen.cppm
            codegen/emitter.cppm
            codegen/eventgen.cppm
            codegen/predicategen.cppm
        PRIVATE
            codegen/eventgen.cpp
            codegen/emitter.cpp
            codegen/predicategen.cpp
)
target_link_libraries(aion_codegen PUBLIC aion_automata)

# --- THE EXECUTABLE ---
add_executable(aionc main.cpp)

# Link the executable to the highest level lib (or all if needed)
target_link_libraries(aionc PRIVATE aion_codegen)

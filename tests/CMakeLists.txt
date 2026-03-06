include(FetchContent)
include(GoogleTest)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(googletest)

foreach(gtest_target gtest gtest_main gmock gmock_main)
    if(TARGET ${gtest_target})
        target_compile_options(${gtest_target} PRIVATE -Wno-error -Wno-character-conversion)
    endif()
endforeach()

add_executable(aion_unit_tests
    unit/frontend_lexer_tests.cpp
    unit/frontend_parser_tests.cpp
    unit/frontend_symbol_table_tests.cpp
    unit/analysis_alphabet_tests.cpp
    unit/automata_glushkov_tests.cpp
)

target_link_libraries(aion_unit_tests
    PRIVATE
        aion_frontend
        aion_analysis
        aion_automata
        GTest::gtest_main
)

gtest_discover_tests(aion_unit_tests NO_PRETTY_VALUES TRUE)

add_executable(aion_contract_tests
    contract/contract_pipeline_tests.cpp
    contract/contract_dump_tests.cpp
    contract/contract_cli_logging_tests.cpp
)

target_link_libraries(aion_contract_tests
    PRIVATE
        aion_utils
        aion_frontend
        aion_analysis
        aion_automata
        GTest::gtest_main
)

gtest_discover_tests(aion_contract_tests NO_PRETTY_VALUES TRUE)

add_executable(aion_e2e_tests
    e2e/e2e_compiler_tests.cpp
)

target_link_libraries(aion_e2e_tests
    PRIVATE
        GTest::gtest_main
)

gtest_discover_tests(aion_e2e_tests NO_PRETTY_VALUES TRUE)

add_executable(aion_semantic_tests
    semantic/semantic_validity_tests.cpp
)

target_link_libraries(aion_semantic_tests
    PRIVATE
        aion_utils
        aion_frontend
        GTest::gtest_main
)

gtest_discover_tests(aion_semantic_tests NO_PRETTY_VALUES TRUE)

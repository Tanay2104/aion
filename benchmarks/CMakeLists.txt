include(FetchContent)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.1
)
FetchContent_MakeAvailable(googlebenchmark)

set(AION_BENCH_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${AION_BENCH_GENERATED_DIR}")

function(aion_generate_bench_header out_var regex_name mode)
    set(regex_path "${CMAKE_CURRENT_SOURCE_DIR}/regex/${regex_name}.regex")
    set(out_base "${AION_BENCH_GENERATED_DIR}/${regex_name}_${mode}")
    set(out_header "${out_base}.hpp")

    set(extra_args)
    if (mode STREQUAL "nojitter")
        list(APPEND extra_args -nj)
    endif()

    add_custom_command(
        OUTPUT "${out_header}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${AION_BENCH_GENERATED_DIR}"
        COMMAND $<TARGET_FILE:aionc> "${regex_path}" -V0 -o "${out_base}" ${extra_args}
        DEPENDS aionc "${regex_path}"
        COMMENT "Generating benchmark runtime header: ${regex_name} (${mode})"
        VERBATIM
    )

    set(${out_var} "${out_header}" PARENT_SCOPE)
endfunction()

aion_generate_bench_header(GROUP_S_JITTER_HEADER group_s jitter)
aion_generate_bench_header(GROUP_S_NOJITTER_HEADER group_s nojitter)
aion_generate_bench_header(GROUP_P_JITTER_HEADER group_p jitter)
aion_generate_bench_header(GROUP_K_JITTER_HEADER group_k jitter)
aion_generate_bench_header(GROUP_K_NOJITTER_HEADER group_k nojitter)
aion_generate_bench_header(GROUP_W_JITTER_HEADER group_w jitter)
aion_generate_bench_header(GROUP_W_NOJITTER_HEADER group_w nojitter)
aion_generate_bench_header(GROUP_R_JITTER_HEADER group_r jitter)
aion_generate_bench_header(GROUP_R_NOJITTER_HEADER group_r nojitter)

add_custom_target(aion_bench_generated_headers
    DEPENDS
        "${GROUP_S_JITTER_HEADER}"
        "${GROUP_S_NOJITTER_HEADER}"
        "${GROUP_P_JITTER_HEADER}"
        "${GROUP_K_JITTER_HEADER}"
        "${GROUP_K_NOJITTER_HEADER}"
        "${GROUP_W_JITTER_HEADER}"
        "${GROUP_W_NOJITTER_HEADER}"
        "${GROUP_R_JITTER_HEADER}"
        "${GROUP_R_NOJITTER_HEADER}"
)

set(AION_BENCH_COMMON_SOURCES
    src/common/bench_common.cpp
)

add_executable(aion_bench_throughput_jitter
    src/throughput/main.cpp
    src/throughput/group_s_jitter.cpp
    src/throughput/group_p.cpp
    src/throughput/group_k_jitter.cpp
    src/throughput/group_w_jitter.cpp
    src/throughput/group_r.cpp
    ${AION_BENCH_COMMON_SOURCES}
)
target_link_libraries(aion_bench_throughput_jitter PRIVATE benchmark::benchmark)
target_include_directories(aion_bench_throughput_jitter PRIVATE src "${AION_BENCH_GENERATED_DIR}")
add_dependencies(aion_bench_throughput_jitter aion_bench_generated_headers)

add_executable(aion_bench_throughput_nojitter
    src/throughput/main.cpp
    src/throughput/group_s_nojitter.cpp
    src/throughput/group_k_nojitter.cpp
    src/throughput/group_w_nojitter.cpp
    src/throughput/group_r_nojitter.cpp
    ${AION_BENCH_COMMON_SOURCES}
)
target_link_libraries(aion_bench_throughput_nojitter PRIVATE benchmark::benchmark)
target_include_directories(aion_bench_throughput_nojitter PRIVATE src "${AION_BENCH_GENERATED_DIR}")
add_dependencies(aion_bench_throughput_nojitter aion_bench_generated_headers)

add_executable(aion_bench_latency_jitter
    src/latency/main.cpp
    src/latency/group_k_jitter.cpp
    src/latency/group_w_jitter.cpp
    ${AION_BENCH_COMMON_SOURCES}
)
target_include_directories(aion_bench_latency_jitter PRIVATE src "${AION_BENCH_GENERATED_DIR}")
add_dependencies(aion_bench_latency_jitter aion_bench_generated_headers)

add_executable(aion_bench_latency_nojitter
    src/latency/main.cpp
    src/latency/group_k_nojitter.cpp
    src/latency/group_w_nojitter.cpp
    ${AION_BENCH_COMMON_SOURCES}
)
target_include_directories(aion_bench_latency_nojitter PRIVATE src "${AION_BENCH_GENERATED_DIR}")
add_dependencies(aion_bench_latency_nojitter aion_bench_generated_headers)

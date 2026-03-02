add_library(aion_runtime STATIC)

target_sources(aion_runtime
    PUBLIC
    FILE_SET CXX_MODULES 
    FILES
        runtime.cppm
)

# Enable AVX2 specific instructions for this target
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(aion_runtime PRIVATE -mavx2)
elseif(MSVC)
    target_compile_options(aion_runtime PRIVATE /arch:AVX2)
endif()

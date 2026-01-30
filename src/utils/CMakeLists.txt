add_library(aion_utils STATIC)
target_sources(aion_utils PUBLIC
        FILE_SET CXX_MODULES FILES
        utils.cppm
        argparser.cppm
)
target_link_libraries(aion_utils PUBLIC aion_core)

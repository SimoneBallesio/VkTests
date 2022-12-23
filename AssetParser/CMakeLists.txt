# Source files
file(GLOB_RECURSE PARSER_HEADERS src/*.hpp)
file(GLOB_RECURSE PARSER_SOURCES src/*.cpp)

add_executable(AssetParser ${PARSER_HEADERS} ${PARSER_SOURCES})
target_include_directories(AssetParser PRIVATE src)
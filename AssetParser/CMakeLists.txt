# Source files
file(GLOB_RECURSE PARSER_HEADERS src/*.hpp)
file(GLOB_RECURSE PARSER_SOURCES src/*.cpp)
file(GLOB_RECURSE STB_IMAGE_HEADERS vendor/stb_image/*.h)
file(GLOB_RECURSE STB_IMAGE_SOURCES vendor/stb_image/*.cpp)

add_executable(AssetParser ${PARSER_HEADERS} ${PARSER_SOURCES} ${STB_IMAGE_HEADERS} ${STB_IMAGE_SOURCES})
target_include_directories(AssetParser PRIVATE src)

# AssetLibrary
target_include_directories(AssetParser PRIVATE ../AssetLibrary/include)
target_link_libraries(AssetParser PRIVATE AssetLibrary)

# stb_image
target_include_directories(AssetParser PRIVATE vendor/stb_image)

# tinyobjloader
add_subdirectory(vendor/tinyobjloader)
target_include_directories(AssetParser PRIVATE vendor/tinyobjloader)
target_link_libraries(AssetParser PRIVATE tinyobjloader)
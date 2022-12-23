# Source files
file(GLOB_RECURSE ASSET_LIB_HEADERS src/*.hpp include/*.hpp)
file(GLOB_RECURSE ASSET_LIB_SOURCES src/*.cpp)

add_library(AssetLibrary STATIC ${ASSET_LIB_HEADERS} ${ASSET_LIB_SOURCES})
set_target_properties(AssetLibrary PROPERTIES PUBLIC_HEADER include/AssetLibrary.hpp)
target_include_directories(AssetLibrary PRIVATE src)

# json
add_subdirectory(vendor/json)
target_include_directories(AssetLibrary PRIVATE vendor/json/include/)
target_link_libraries(AssetLibrary PRIVATE nlohmann_json)

# lz4
file(GLOB LZ4_HEADERS vendor/lz4/lib/lz4.h
											vendor/lz4/lib/lz4file.h
											vendor/lz4/lib/lz4frame_static.h
											vendor/lz4/lib/lz4frame.h
											vendor/lz4/lib/lz4hc.h
											vendor/lz4/lib/xxhash.h)

file(GLOB LZ4_SOURCES vendor/lz4/lib/lz4.c
											vendor/lz4/lib/lz4file.c
											vendor/lz4/lib/lz4frame.c
											vendor/lz4/lib/lz4hc.c
											vendor/lz4/lib/xxhash.c)

add_library(lz4 STATIC ${LZ4_HEADERS} ${LZ4_SOURCES})
target_include_directories(lz4 PRIVATE vendor/lz4/lib)
target_include_directories(AssetLibrary PRIVATE vendor/lz4/lib)
target_link_libraries(AssetLibrary PRIVATE lz4)
########################################################################
## Feature registration
########################################################################
cmake_dependent_option(ENABLE_BLOCKS_SERIALIZE "Enable Pothos Blocks.Serialize component" ON "ENABLE_BLOCKS;JSON_HPP_INCLUDE_DIR" OFF)
add_feature_info("  Serialize" ENABLE_BLOCKS_SERIALIZE "Serialize data to/from byte stream")
if (NOT ENABLE_BLOCKS_SERIALIZE)
    return()
endif()

########################################################################
# Serialize blocks module
########################################################################
include_directories(${JSON_HPP_INCLUDE_DIR})
POTHOS_MODULE_UTIL(
    TARGET SerializeBlocks
    SOURCES
        Serializer.cpp
        Deserializer.cpp
        TestSerialize.cpp
    DESTINATION blocks
    ENABLE_DOCS
)

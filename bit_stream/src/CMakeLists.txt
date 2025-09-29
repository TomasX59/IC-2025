cmake_minimum_required(VERSION 3.16)
project(bit_stream)

SET (CMAKE_BUILD_TYPE "Release")
#SET (CMAKE_BUILD_TYPE "Debug")

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++20")
SET (CMAKE_CXX_FLAGS_RELEASE "-O3")
SET (CMAKE_CXX_FLAGS_DEBUG "-g3 -fsanitize=address")

SET (BASE_DIR ${CMAKE_SOURCE_DIR} )
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BASE_DIR}/../bin)

add_library(Common OBJECT)

target_sources(Common PRIVATE bit_stream.cpp byte_stream.cpp)

add_executable (text2bin text2bin.cpp $<TARGET_OBJECTS:Common>)
add_executable (bin2text bin2text.cpp $<TARGET_OBJECTS:Common>)


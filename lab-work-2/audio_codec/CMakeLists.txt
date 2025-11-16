cmake_minimum_required(VERSION 3.16)
project(audio_codec)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED sndfile)

add_executable(audio_codec audio_codec.cpp ../golomb/Golomb.cpp)
target_include_directories(audio_codec PRIVATE ${SNDFILE_INCLUDE_DIRS} ../golomb)
target_link_libraries(audio_codec PRIVATE ${SNDFILE_LIBRARIES})

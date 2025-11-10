cmake_minimum_required(VERSION 3.16)
project(trabalho2)

set(CMAKE_BUILD_TYPE "Release")
# set(CMAKE_BUILD_TYPE "Debug")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -fsanitize=address")

# Output directory for executables
set(BASE_DIR ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BASE_DIR}/bin)


# ---- OpenCV (for image manipulation) ----
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS}) 

# ---- SndFile (for audio I/O) ----
find_path(SNDFILE_INCLUDE_DIR sndfile.h)
find_library(SNDFILE_LIBRARY NAMES sndfile)

if(NOT SNDFILE_INCLUDE_DIR OR NOT SNDFILE_LIBRARY)
    message(FATAL_ERROR "libsndfile header (sndfile.h) or library (libsndfile) not found. Please ensure the libsndfile development package is installed (e.g., 'libsndfile1-dev' on Debian/Ubuntu).")
endif()


# ex1
add_executable(ex1 ex1.cpp)
target_link_libraries(ex1 ${OpenCV_LIBS} ${SNDFILE_LIBRARY})

#ex2
add_executable(ex2 ex2.cpp)
target_link_libraries(ex2 ${OpenCV_LIBS} ${SNDFILE_LIBRARY})

# ex3
add_executable(ex3
    ex3.cpp
    golomb.cpp
    bit_stream.cpp
    byte_stream.cpp
)

#ex4
add_executable(
    audio_golomb_codec
    ex4.cpp
    audio_codec.cpp
    audio_codec.h
    golomb.cpp
    golomb.h
    bit_stream.cpp
    bit_stream.h
    byte_stream.cpp
    byte_stream.h
)
target_include_directories(audio_golomb_codec PUBLIC ${SNDFILE_INCLUDE_DIR})
target_link_libraries(audio_golomb_codec PUBLIC ${SNDFILE_LIBRARY})

#ex5
add_executable(
    image_golomb_codec
    ex5.cpp
    image_codec.cpp
    image_codec.h
    golomb.cpp
    golomb.h
    bit_stream.cpp
    bit_stream.h
    byte_stream.cpp
    byte_stream.h
)
target_link_libraries(image_golomb_codec ${OpenCV_LIBS})
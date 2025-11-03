cmake_minimum_required(VERSION 3.16)
project(trabalho2)

# ============================================================
#  Build configuration
# ============================================================
set(CMAKE_BUILD_TYPE "Release")
# set(CMAKE_BUILD_TYPE "Debug")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -fsanitize=address")

# Output directory for executables
set(BASE_DIR ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BASE_DIR}/../bin)

# ============================================================
#  Dependencies
# ============================================================

# ---- OpenCV (for image manipulation) ----
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# ============================================================
#  Executables
# ============================================================

# ex1 (example using both audio and image processing)
add_executable(ex1 ex1.cpp)
target_link_libraries(ex1 ${OpenCV_LIBS} sndfile)

# ex2 (another exercise — also link both just in case)
add_executable(ex2 ex2.cpp)
target_link_libraries(ex2 ${OpenCV_LIBS} sndfile)

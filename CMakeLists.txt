cmake_minimum_required(VERSION 3.20)
project(hw2)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")

file(GLOB SRCS "src/ser/*.cpp" "src/*.cpp")
add_executable("server" ${SRCS})

file(GLOB SRCS "src/cli/*.cpp" "src/*.cpp")
file(GLOB_RECURSE IMGUI "lib/imgui/*.cpp")
include_directories("inc")
link_directories("lib/SDL3")
add_executable("client" ${SRCS} ${IMGUI})
target_link_libraries("client" -lSDL3)

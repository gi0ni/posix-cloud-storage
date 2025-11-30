cmake_minimum_required(VERSION 3.20)
project(hw2)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")

find_package(SDL2)
find_package(tinyxml2 REQUIRED)

include_directories("inc")

file(GLOB SRCS "src/ser/*.cpp" "src/*.cpp")
add_executable("server" ${SRCS})
target_link_libraries("server" tinyxml2::tinyxml2 -lsodium)

file(GLOB SRCS "src/cli/*.cpp" "src/*.cpp")
file(GLOB_RECURSE IMGUI "lib/imgui/*.cpp")
add_executable("client" ${SRCS} ${IMGUI})
target_link_libraries("client" SDL2::SDL2 -lsodium)

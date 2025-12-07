cmake_minimum_required(VERSION 3.20)
project(hw2)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")

find_package(SDL2 REQUIRED)
find_package(tinyxml2 REQUIRED)
find_package(OpenSSL REQUIRED)

include_directories("inc" "src/com")
file(GLOB COMMON_SRCS "src/com/*.cpp")
file(GLOB SERVER_SRCS "src/ser/*.cpp")
file(GLOB CLIENT_SRCS "src/cli/*.cpp")
file(GLOB_RECURSE IMGUI "lib/imgui/*.cpp")

add_executable("server" ${SERVER_SRCS} ${COMMON_SRCS})
target_link_libraries("server" tinyxml2::tinyxml2 -lsodium OpenSSL::Crypto)

add_executable("client" ${CLIENT_SRCS} ${COMMON_SRCS} ${IMGUI})
target_link_libraries("client" SDL2::SDL2 -lsodium OpenSSL::Crypto)

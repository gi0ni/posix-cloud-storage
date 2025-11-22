cmake_minimum_required(VERSION 3.20)
project(hw2)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")

set(SRCS 
	"src/server.cpp"
	"src/utils.cpp"
	"src/packet.cpp"
)

add_executable("server" ${SRCS})

set(SRCS
	"src/client.cpp"
	"src/utils.cpp"
	"src/packet.cpp"
)

file(GLOB_RECURSE IMGUI "lib/imgui/*.cpp")
include_directories("inc")
link_directories("lib/SDL3")

add_executable("client" ${SRCS} ${IMGUI})
target_link_libraries("client" -lSDL3)

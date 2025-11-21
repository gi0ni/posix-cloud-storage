cmake_minimum_required(VERSION 3.20)
project(hw2)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")

set(SRCS 
	"src/server.cpp"
)

add_executable("server" ${SRCS})

set(SRCS
	"src/client.cpp"
)

add_executable("client" ${SRCS})

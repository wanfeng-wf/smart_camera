cmake_minimum_required(VERSION 3.10)

project(server LANGUAGES C)

add_executable(server
	src/main.c
	src/camera.c
	src/app.c
	src/node.c
	src/cJSON.c
)

target_include_directories(server PRIVATE src)
target_link_libraries(server pthread)

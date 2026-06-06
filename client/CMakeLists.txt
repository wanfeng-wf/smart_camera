cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER arm-openwrt-linux-gcc)

project(client LANGUAGES C)

add_executable(client
	src/main.c
	src/video.c
	src/motor.c
	src/cJSON.c
)

target_include_directories(client PRIVATE src)
target_link_libraries(client pthread)

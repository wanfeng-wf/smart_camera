cmake_minimum_required(VERSION 3.10)

project(client LANGUAGES C)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)

add_executable(client src/main.c src/cJSON.c)

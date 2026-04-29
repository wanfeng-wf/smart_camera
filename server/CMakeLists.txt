cmake_minimum_required(VERSION 3.10)

project(server LANGUAGES C)

add_executable(server src/main.c src/protocol.c src/cJSON.c)

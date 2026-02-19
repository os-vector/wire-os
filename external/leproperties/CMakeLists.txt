cmake_minimum_required(VERSION 3.10)
project(leproperties C)

set(CMAKE_C_STANDARD 99)

add_executable(leproperties server.c)
add_executable(getprop getprop.c)
add_executable(setprop setprop.c)

target_compile_options(leproperties PRIVATE -Wall -Wextra)
target_compile_options(getprop PRIVATE -Wall -Wextra)
target_compile_options(setprop PRIVATE -Wall -Wextra)

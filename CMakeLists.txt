# cmake_minimum_required(VERSION <specify CMake version here>)
project(flyf C)

set(CMAKE_C_STANDARD 11)

aux_source_directory(. DIR_SRCS)

add_executable(ftp ${DIR_SRCS})
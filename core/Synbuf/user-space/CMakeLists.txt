cmake_minimum_required(VERSION 3.15)
project(user_space C)

set(CMAKE_C_STANDARD 11)

add_definitions(-DDEVNAME="moses")

add_library(synbuf-connector STATIC
        include/synbuf-connector.h
        src/synbuf-connector.c)


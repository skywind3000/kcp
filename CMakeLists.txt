PROJECT(kcp)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

add_library(kcp STATIC ikcp.c)

add_executable(kcp_test test.cpp)






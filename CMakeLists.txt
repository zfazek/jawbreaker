project(jawbreaker)

cmake_minimum_required(VERSION 2.8)

set (CMAKE_CXX_FLAGS "-std=c++11 -g -O3 -Werror -Wall")

add_executable(jawbreaker.cgi
    jawbreaker.cc
    )

target_link_libraries(jawbreaker.cgi
    cgicc
    )

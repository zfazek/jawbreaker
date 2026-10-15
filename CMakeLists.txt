cmake_minimum_required(VERSION 3.10)

project(jawbreaker)

set (CMAKE_CXX_FLAGS "-std=c++11 -g -O3 -Werror -Wall -Wextra -Wshadow")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(jawbreaker.cgi
    jawbreaker.cc
    )

target_link_libraries(jawbreaker.cgi
    cgicc
    Threads::Threads
    )

# Install the compiled CGI binary into the web server's cgi-bin directory.
# Defaults to the Debian/Ubuntu Apache2 location (/usr/lib/cgi-bin), which maps
# to the "/cgi-bin/" URL the front end requests. Override on other distros, e.g.
#   cmake -DCGI_BIN_DIR=/var/www/cgi-bin ..   (RHEL/CentOS/Fedora httpd)
set(CGI_BIN_DIR "/usr/lib/cgi-bin"
    CACHE PATH "Destination directory for the installed CGI binary")

install(TARGETS jawbreaker.cgi
    RUNTIME DESTINATION "${CGI_BIN_DIR}"
    )

# ---------------------------------------------------------------------------
# Tests. A single `ctest` runs both the C++ and the JavaScript cross-language
# equivalence tests, which both check the shared tests/fixtures.json so the two
# implementations (board.h and main.js) cannot silently diverge.
#
#   mkdir build && cd build && cmake .. && make && ctest --output-on-failure
# ---------------------------------------------------------------------------
enable_testing()

set(FIXTURES "${CMAKE_SOURCE_DIR}/tests/fixtures.json")

# C++ equivalence test: includes board.h, applies fixtures, asserts results.
add_executable(board_test tests/board_test.cc)
target_include_directories(board_test PRIVATE "${CMAKE_SOURCE_DIR}")
add_test(NAME cpp_equivalence COMMAND board_test "${FIXTURES}")

# JavaScript equivalence test using Node's built-in test runner. Registered
# only if node is available, so the build still works on systems without it.
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
    add_test(NAME js_equivalence
        COMMAND "${NODE_EXECUTABLE}" --test "${CMAKE_SOURCE_DIR}/tests/game.test.mjs")
else()
    message(STATUS "node not found: skipping js_equivalence test")
endif()

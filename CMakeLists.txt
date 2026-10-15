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

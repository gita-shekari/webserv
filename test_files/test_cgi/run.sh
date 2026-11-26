#!/bin/sh
set -eu

# Build only CGI and its logger; no server loop or listening sockets are needed.
cd "$(dirname "$0")/../.."
python_path=$(command -v python3)
test_binary=$(mktemp "${TMPDIR:-/tmp}/webserv-cgi-test.XXXXXX")
trap 'rm -f "$test_binary"' EXIT HUP INT TERM

c++ -Wall -Wextra -Werror -std=c++11 -Iinclude \
	test_files/test_cgi/test_cgi.cpp src/CGI.cpp src/Logger.cpp \
	-o "$test_binary"

# Error logs are expected for the deliberately invalid requests.
"$test_binary" "$python_path"

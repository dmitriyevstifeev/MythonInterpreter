cmake_minimum_required(VERSION 3.20)
project(MythonInterpreter)

set(CMAKE_CXX_STANDARD 17)
add_compile_options(-O3 -Wall -Wextra -Werror -march=native -mtune=native -fsanitize=address)
add_link_options(-fsanitize=address)
add_executable(MythonInterpreter main.cpp lexer.cpp lexer.h lexer_test_open.cpp parse.cpp parse.h parse_test.cpp runtime.h runtime.cpp runtime_test.cpp statement.cpp statement.h statement_test.cpp test_runner_p.h)
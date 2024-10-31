cmake_minimum_required(VERSION 3.29.0)

project(Maestro)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SOURCES src/bitboard.cpp
            src/position.cpp
            src/movegen.cpp
            src/perft.cpp
            src/hash.cpp
            src/polyglot.cpp
            src/search.cpp
            src/eval.cpp
            src/thread.cpp
            src/uci.cpp
            src/main.cpp
            )
set(HEADERS src/defs.hpp
            src/bitboard.hpp
            src/utils.hpp
            src/position.hpp
            src/movegen.hpp
            src/move.hpp
            src/hash.hpp
            src/perft.hpp
            src/polyglot.hpp
            src/search.hpp
            src/eval.hpp
            src/thread.hpp
            src/uci.hpp
            )

set(USER_FLAGS "-g -Ofast -march=native -lpthread -DUSE_AVX -mavx2 -DUSE-SSE41 -msse4.1 -DUSE_SSSE3 -mssse3 -DUSE_SSE2 -msse2 -DUSE_SSE -msse")

set(EXECUTABLE_OUTPUT_PATH ../bin)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${USER_FLAGS} ")

add_executable(Maestro ${HEADERS} ${SOURCES})
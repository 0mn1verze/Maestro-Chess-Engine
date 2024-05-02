cmake_minimum_required(VERSION 3.29.0)

project(Maestro)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SOURCES src/bitboard.cpp
            src/position.cpp
            src/movegen.cpp
            src/perft.cpp
            src/uci.cpp
            src/eval.cpp
            src/hashtable.cpp
            src/misc.cpp
            src/movepicker.cpp
            src/nnue.cpp
            src/search.cpp
            src/polyglot.cpp
            src/main.cpp)
set(HEADERS src/defs.hpp
            src/bitboard.hpp
            src/utils.hpp
            src/position.hpp
            src/movegen.hpp
            src/uci.hpp
            src/eval.hpp
            src/hashtable.hpp
            src/misc.hpp
            src/movepicker.hpp
            src/nnue.hpp
            src/search.hpp
            src/polyglot.hpp
            src/perft.hpp)

# set(USER_FLAGS "-g -Ofast -march=native -lpthread -DUSE_AVX -mavx2 -DUSE-SSE41 -msse4.1 -DUSE_SSSE3 -mssse3 -DUSE_SSE2 -msse2 -DUSE_SSE -msse")

set(USER_FLAGS "-g -Ofast -DUSE-MMX -mavx")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${USER_FLAGS} ")

set(EXECUTABLE_OUTPUT_PATH ../bin)

add_executable(Maestro ${SOURCES} ${HEADERS})
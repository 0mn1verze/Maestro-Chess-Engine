cmake_minimum_required(VERSION 3.16.3)

project(Maestro)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SOURCES src/bitboard.cpp
            src/position.cpp
            src/movegen.cpp
            src/movepicker.cpp
            src/history.cpp
            src/perft.cpp
            src/hash.cpp
            src/nnue.cpp
            src/misc.cpp
            src/search.cpp
            src/thread.cpp
            src/eval.cpp
            src/engine.cpp
            src/uci.cpp
            src/polyglot.cpp
            src/main.cpp
            )
set(HEADERS src/defs.hpp
            src/incbin.hpp
            src/bitboard.hpp
            src/utils.hpp
            src/position.hpp
            src/movegen.hpp
            src/move.hpp
            src/movepicker.hpp
            src/history.hpp
            src/hash.hpp
            src/nnue.hpp
            src/misc.hpp
            src/perft.hpp
            src/search.hpp
            src/thread.hpp
            src/eval.hpp
            src/engine.hpp
            src/polyglot.hpp
            src/uci.hpp
            )

set(USER_FLAGS "-g -Ofast -flto -pthread -DUSE_AVX2 -mavx2")

set(EXECUTABLE_OUTPUT_PATH ../bin)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${USER_FLAGS} ")

add_executable(Maestro ${HEADERS} ${SOURCES})

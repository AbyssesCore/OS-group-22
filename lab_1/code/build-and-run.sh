#!/bin/bash

echo "(Bash): Formatting source files..."
clang-format -i lsh.c parse.c parse.h

echo "(Bash): Building lsh..."
cmake -B build
cmake --build build --verbose

echo "(Bash): Running lsh..."
./build/lsh
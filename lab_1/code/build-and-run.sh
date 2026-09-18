#!/bin/bash

echo "(Bash): Building lsh..."
cmake -B build
cmake --build build --verbose

echo "(Bash): Running lsh..."
./build/lsh
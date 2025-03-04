#!/bin/bash

CXX=clang++
CXXFLAGS="-Wall"
LDFLAGS="-lX11 -lXfixes -lcairo -O3"

TARGET="raindrop"
SRCS="raindrop.cpp"

echo "Compiling and linking $SRCS..."
$CXX $CXXFLAGS $SRCS -o $TARGET $LDFLAGS

if [ $? -eq 0 ]; then
    echo "Build complete. Running the executable..."
    ./raindrop 
else
    echo "Build failed."
fi

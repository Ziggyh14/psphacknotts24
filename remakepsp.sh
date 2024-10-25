#!/bin/bash
rm -r build
mkdir build && cd build
psp-cmake ..
make


#!/bin/bash

cd src/
mkdir -p build && cd build
cmake ../ && make -j4

#!/bin/bash

cd ../../examples/BasicExample/
echo "Building Example 'BasicExample' at [${PWD}/BasicExample]"
rm -f BasicExample
g++ -I../../src/ -I../../examples/External/ -I../../examples/External/raylib/ -L../../examples/External/raylib/ -o BasicExample BasicExample.cpp ../../src/fVoxel.cpp -lraylib -ggdb
cd ../../build/Examples/



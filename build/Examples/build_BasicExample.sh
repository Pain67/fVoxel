#!/bin/bash

cd ../../examples/BasicExample/
echo "Building Example 'BasicExample' at [${PWD}/BasicExample]"
g++ -I../../src/ -I../External/ -lraylib -lm -lc -o BasicExample BasicExample.cpp
cd ../../build/Examples/



echo "Building shared library libfVoxel.so"
g++ -c -fpic -o ../../src/fVoxel.o ../../src/fVoxel.cpp
g++ -shared -o ../../src/libfVoxel.so ../../src/fVoxel.o
rm ../../src/fVoxel.o

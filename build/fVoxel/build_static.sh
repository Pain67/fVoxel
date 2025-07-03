echo "Building static library libfVoxel.a"
g++ -c -o ../../src/fVoxel.o ../../src/fVoxel.cpp
ar rcs ../../src/libfVoxel.a ../../src/fVoxel.o
rm ../../src/fVoxel.o

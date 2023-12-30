time -p cmake  -S$PWD -B$PWD/build/debug \
 -D CMAKE_CXX_COMPILER=g++-13 -DBUILDING_LIBCURL=1 -DCMAKE_BUILD_TYPE=Debug .
cd build/debug/
time -p make -j8

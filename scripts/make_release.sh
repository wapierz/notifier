time -p cmake  -S$PWD -B$PWD/build/release \
 -D CMAKE_CXX_COMPILER=g++-13 -DBUILDING_LIBCURL=1 -DCMAKE_BUILD_TYPE=Release .
cd build/release/
time -p make -j8

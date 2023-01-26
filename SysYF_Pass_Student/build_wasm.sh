rm -rf build
mkdir build
cd build
emcmake cmake -DBUILD_WASM=ON ..
emmake make -j
#!/bin/bash
set -euf -o pipefail
mkdir -p build
pushd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
ctest . -C Release
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
ctest . -C Debug
popd


#!/bin/zsh
# Prepare
#git submodule update --init --recursive
#pip3 install wheel setuptools
#pip3 install aqtinstall conan
# Install Qt
#aqt install-qt mac desktop 6.2.1 -O ./build/Qt
# Instal other dependencies using conan
#conan install -if build --build missing .
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SERVICE=OFF -DCMAKE_MODULE_PATH=./build/Qt6/6.2.1/macos/lib/cmake -DQt6_DIR=./build/Qt6/6.2.1/macos/lib/cmake/Qt6 -DCODESIGN_CERTIFICATE_NAME="Developer ID Application: Tobias Hegemann (JH3275598G)"
# Build
cmake --build build --config Release --parallel
# Pack
cpack --config build/CPackConfigApp.cmake -B build
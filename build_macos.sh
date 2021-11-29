#!/bin/zsh
git submodule update --init --recursive
sudo pip3 install aqtinstall
aqt install-qt mac desktop 6.2.1 -O ./build/Qt
conan install -if build --build missing .
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SERVICE=OFF -DCMAKE_MODULE_PATH=./build/Qt6/6.2.1/macos/lib/cmake -DCODESIGN_CERTIFICATE_NAME="Developer ID Application: Tobias Hegemann (JH3275598G)"
cmake --build build --config Release --parallel
cpack --config build/CPackConfigApp.cmake -B build
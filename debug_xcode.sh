#!/bin/zsh
## You can skip preparation by calling ./build_macos.sh noprepare
if [ "$1" != "onlybuild" ]; then
  # Prepare
  #git submodule update --init --recursive
  pip3 install wheel setuptools
  pip3 install conan

  # Install other dependencies using conan
  conan install -if build -s build_type=Debug --build=missing .
  # Configure
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -G Xcode
fi
# Build
cmake --build build --config Debug

if [ "$1" = "pack" ]; then
  # Pack app
  cpack --config build/CPackConfigApp.cmake -B build
  # Pack service
  cpack --config build/CPackConfigService.cmake -B build
fi
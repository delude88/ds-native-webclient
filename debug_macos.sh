#!/bin/zsh
## You can skip preparation by calling ./build_macos.sh noprepare
if [ "$1" != "onlybuild" ]; then
  # Prepare
  #git submodule update --init --recursive
  pip3 install wheel setuptools
  pip3 install conan

  # Install other dependencies using conan
  conan install -if cmake-build-debug -s build_type=Debug --build=missing .
  # Configure
  cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
fi
# Build
cmake --build cmake-build-debug --config Release

if [ "$1" = "pack" ]; then
  # Pack app
  cpack --config build/CPackConfigApp.cmake -B cmake-build-debug
  # Pack service
  cpack --config build/CPackConfigService.cmake -B cmake-build-debug
fi
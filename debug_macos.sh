#!/bin/zsh
## You can skip preparation by calling ./build_macos.sh noprepare
if [ "$1" != "onlybuild" ]; then
  # Prepare
  HOMEBREW_NO_AUTO_UPDATE=1 brew install python3 include-what-you-use
  #git submodule update --init --recursive
  pip3 install wheel setuptools
  pip3 install conan

  # Install other dependencies using conan
  conan install -if cmake-build-debug -s build_type=Debug --build=missing .
  # Configure
  cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_RUN_IWYU=On
fi
# Build
cmake --build cmake-build-debug --config Debug -j 4 2> iwyu.out

# May use this to fix up includes (macOS M1 homebrew):
# python3 /opt/homebrew/Cellar/include-what-you-use/0.17/libexec/bin/fix_includes.py < iwyu.out

if [ "$1" = "pack" ]; then
  # Pack app
  cpack --config build/CPackConfigApp.cmake -B cmake-build-debug
  # Pack service
  cpack --config build/CPackConfigService.cmake -B cmake-build-debug
fi
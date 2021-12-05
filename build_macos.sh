#!/bin/zsh
## You can skip preparation by calling ./build_macos.sh noprepare
if [ "$1" != "noprepare" ]; then
  # Prepare
  git submodule update --init --recursive
  pip3 install wheel setuptools
  pip3 install conan

  # Install other dependencies using conan
  conan install -if build --build missing .
  # Configure
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13 -DCODESIGN_CERTIFICATE_NAME="Developer ID Application: Tobias Hegemann (JH3275598G)"
fi
# Build
cmake --build build --config Release
# Pack app
cpack --config build/CPackConfigApp.cmake -B build
# Pack service
cpack --config build/CPackConfigService.cmake -B build
codesign --force --options runtime --sign "Developer ID Application: Tobias Hegemann (JH3275598G)" build/digital-stage-connector*.dmg
spctl -a -t open -vvv --context context:primary-signature build/digital-stage-connector*.dmg

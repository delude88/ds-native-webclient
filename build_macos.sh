#!/bin/zsh
## You can skip preparation by calling ./build_macos.sh noprepare
if [ "$1" != "onlybuild" ]; then
  # Prepare
  #git submodule update --init --recursive
  pip3 install wheel setuptools
  pip3 install conan

  # Install other dependencies using conan
  conan install -if cmake-build-release --build missing .
  # Configure
  cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release -DCODESIGN_CERTIFICATE_NAME="Developer ID Application: Tobias Hegemann (JH3275598G)"
fi
# Build
cmake --build cmake-build-release --config Release

# Pack app
cpack --config cmake-build-release/CPackConfigApp.cmake -B cmake-build-release
# Pack service
cpack --config cmake-build-release/CPackConfigService.cmake -B cmake-build-release

if [ "$1" = "sign" ]; then
  xcrun altool --notarize-app -u tobias.hegemann@googlemail.com -p "@keystore:Developer-altool" --primary-bundle-id de.tobiashegemann.digital-stage.app --file cmake-build-release/digital-stage-connector-app-macos-arm64.dmg
  for (( ; ; ))
  do
    xcrun stapler staple cmake-build-release/digital-stage-connector-app-macos-arm64.dmg
    if [ $? -eq 0 ]; then break; fi
    sleep 5s
  done
fi
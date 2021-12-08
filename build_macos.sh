#!/bin/zsh
## You can skip preparation by calling ./build_macos.sh noprepare
if [ "$1" != "onlybuild" ]; then
  # Prepare
  #git submodule update --init --recursive
  pip3 install wheel setuptools
  pip3 install conan

  # Install other dependencies using conan
  conan install -if build --build missing -s os.version=10.13 .
  # Configure
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13 -DCODESIGN_CERTIFICATE_NAME="Developer ID Application: Tobias Hegemann (JH3275598G)"
fi
# Build
cmake --build build --config Release

if [ "$1" = "sign" ]; then
  # Pack app
  cpack --config build/CPackConfigApp.cmake -B build
  # Pack service
  cpack --config build/CPackConfigService.cmake -B build
  xcrun altool --notarize-app -u tobias.hegemann@googlemail.com -p "@keystore:Developer-altool" --primary-bundle-id de.tobiashegemann.digital-stage.app --file build/digital-stage-connector-*.dmg
  for (( ; ; ))
  do
    xcrun stapler staple build/digital-stage-connector*.dmg
    if [ $? -eq 0 ]; then break; fi
  done
  xcrun altool --notarize-app -u tobias.hegemann@googlemail.com -p "@keystore:Developer-altool" --primary-bundle-id de.tobiashegemann.digital-stage.service --file build/digital-stage-connector-*.pkg
  for (( ; ; ))
  do
    xcrun stapler staple build/digital-stage-connector*.pkg
    if [ $? -eq 0 ]; then break; fi
  done
fi
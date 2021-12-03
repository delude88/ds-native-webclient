#!/bin/zsh
## You can skip preparation by calling ./build_macos.sh noprepare
if [ "$1" != "noprepare" ]; then
  # Prepare
  git submodule update --init --recursive
  pip3 install wheel setuptools
  pip3 install conan
  # Install Qt
  #aqt install-qt mac desktop 6.2.1 -O ./build/Qt
  brew install qt
  # Instal other dependencies using conan
  conan install -if build --build missing .
  # Configure
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCODESIGN_CERTIFICATE_NAME="Developer ID Application: Tobias Hegemann (JH3275598G)"
fi
# Build
cmake --build build --config Release --parallel --target DigitalStageConnectorApp
#/opt/homebrew/opt/qt/bin/macdeployqt "build/bin/Digital Stage Connector.app" -verbose=1 -always-overwrite -sign-for-notarization="Developer ID Application: Tobias Hegemann (JH3275598G)"
# Pack app
#cpack --config build/CPackConfigApp.cmake -B build
# Pack service
# cpack --config build/CPackConfigService.cmake -B build
#codesign --force --options runtime --sign "Developer ID Application: Tobias Hegemann (JH3275598G)" build/Install-Connector.dmg
#spctl -a -t open -vvv --context context:primary-signature build/Install-Connector.dmg

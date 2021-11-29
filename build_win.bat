@echo off
git submodule update --init --recursive
pip3 install wheel setuptools
pip3 install conan aqtinstall
aqt install-qt windows desktop 6.2.1 win64_msvc2019_64 -O Qt
conan install -if build --build missing .
set PATH=%PATH%;%cd%\Qt\6.2.1\msvc2019_64\lib\cmake
cmake -S . -B build -DBUILD_SERVICE=OFF -DUSE_GNUTLS=0 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
cpack --config build/CPackConfig.cmake -B build
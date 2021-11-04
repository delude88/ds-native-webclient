#!/bin/bash
BASEDIR=$(pwd)
git submodule update --init --recursive

# Add google tools to path (runtime of this script)
export PATH=$BASEDIR/tools/depot_tools:$PATH

echo "Checking out WebRTC"
mkdir -p include/libwebrtc
cd include/libwebrtc || exit
fetch --nohooks webrtc
gclient sync
cd src || exit
## Uncomment the following lines to support libmediasoupclient (won't run on arm64)
#git checkout -b m84 refs/remotes/branch-heads/4147
#gclient sync

echo "Configuring WebRTC build"
cp $BASEDIR/cmake/rtc_export_template.h.patch $BASEDIR/include/libwebrtc/src/rtc_base/system/
git apply rtc_base/system/rtc_export_template.h.patch
if [[ "$OSTYPE" == "darwin"* ]]; then
        # MacOS
	gn gen out/Default --args='is_debug=false is_component_build=false is_clang=true rtc_include_tests=false rtc_use_h264=true use_rtti=true mac_deployment_target="10.11" use_custom_libcxx=false'
else
        # Linux (or "no macos")
	gn gen out/Default --args='is_debug=false is_component_build=false is_clang=false rtc_include_tests=false rtc_use_h264=true use_rtti=true use_custom_libcxx=false treat_warnings_as_errors=false use_ozone=true'
fi

echo "Building WebRTC"
ninja -C out/Default


cd $BASEDIR || exit
export LIBWEBRTC_INCLUDE_PATH=$BASEDIR/include/libwebrtc/src/
export LIBWEBRTC_BINARY_PATH=$BASEDIR/include/libwebrtc/src/out/Default/obj

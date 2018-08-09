#!/bin/bash -e

ROOT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BOOST_LIBRARYDIR=${ROOT_DIR}/../ofxiOSBoost/build/ios/prefix/lib
BOOST_LIBRARYDIR_x86_64=${ROOT_DIR}/../ofxiOSBoost/build/libs/boost/lib/x86_64
BOOST_INCLUDEDIR=${ROOT_DIR}/../ofxiOSBoost/build/ios/prefix/include
OPENSSL_INCLUDE_DIR=${ROOT_DIR}/../OpenSSL/include
OPENSSL_ROOT_DIR=${ROOT_DIR}/../OpenSSL
INSTALL_PREFIX=${ROOT_DIR}

#mkdir -p build-ios/release

echo "Building IOS armv7"
#rm -r -f build-ios-armv7 > /dev/null
mkdir -p build-ios-armv7/release
pushd build-ios-armv7/release
cmake -D IOS=true -D ARCH=armv7 -D CMAKE_BUILD_TYPE=debug -D BOOST_LIBRARYDIR=${BOOST_LIBRARYDIR} -D BOOST_INCLUDEDIR=${BOOST_INCLUDEDIR} -D OPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR} -D OPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR} -D STATIC=ON -D CMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -D INSTALL_VENDORED_LIBUNBOUND=ON -D BUILD_GUI_DEPS=ON ../..
make -j4
mkdir -p ../../lib-armv7
cp -f lib/libwallet_merged.a ../../lib-armv7
cp -f lib/libwallet.a ../../lib-armv7
cp -f external/unbound/libunbound.a ../../lib-armv7
cp -f contrib/epee/src/libepee.a ../../lib-armv7
#cp -f libeasylogging.a ../../lib-armv7
mkdir -p ../../lib
cp -f external/miniupnpc/libminiupnpc.a ../../lib
popd

echo "Building IOS arm64"
#rm -r build-ios-arm64 > /dev/null
mkdir -p build-ios-arm64/release
pushd build-ios-arm64/release
cmake -D IOS=true -D ARCH=arm64 -D CMAKE_BUILD_TYPE=debug -D BOOST_LIBRARYDIR=${BOOST_LIBRARYDIR} -D BOOST_INCLUDEDIR=${BOOST_INCLUDEDIR} -D OPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR} -D OPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR} -D STATIC=ON -D CMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -D INSTALL_VENDORED_LIBUNBOUND=ON -D BUILD_GUI_DEPS=ON ../..
make -j4
mkdir -p ../../lib-arm64
cp -f lib/libwallet_merged.a ../../lib-arm64
cp -f lib/libwallet.a ../../lib-arm64
cp -f external/unbound/libunbound.a ../../lib-arm64
cp -f contrib/epee/src/libepee.a ../../lib-arm64
#cp -f libeasylogging.a ../../lib-arm64
popd

echo "Building IOS x86"
#rm -r build-ios-x86 > /dev/null
mkdir -p build-ios-x86/release
pushd build-ios-x86/release
cmake -D IOS=true -D ARCH=x86_64 -D IOS_PLATFORM=SIMULATOR64 -D CMAKE_BUILD_TYPE=debug -D BOOST_LIBRARYDIR=${BOOST_LIBRARYDIR_x86_64} -D BOOST_INCLUDEDIR=${BOOST_INCLUDEDIR} -D OPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR} -D OPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR} -D STATIC=ON -D CMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -D INSTALL_VENDORED_LIBUNBOUND=ON -D BUILD_GUI_DEPS=ON ../..
make -j4
mkdir -p ../../lib-x86_64
cp -f lib/libwallet_merged.a ../../lib-x86_64
cp -f lib/libwallet.a ../../lib-x86_64
cp -f external/unbound/libunbound.a ../../lib-x86_64
cp -f contrib/epee/src/libepee.a ../../lib-x86_64
#cp -f libeasylogging.a ../../lib-x86_64
popd

echo "Creating fat library for armv7 and arm64"
mkdir -p lib-ios
lipo -create lib-armv7/libwallet_merged.a lib-x86_64/libwallet_merged.a lib-arm64/libwallet_merged.a -output lib-ios/libwallet_merged.a
lipo -create lib-armv7/libwallet.a lib-x86_64/libwallet.a lib-arm64/libwallet.a -output lib-ios/libwallet.a
lipo -create lib-armv7/libunbound.a lib-x86_64/libunbound.a lib-arm64/libunbound.a -output lib-ios/libunbound.a
lipo -create lib-armv7/libepee.a lib-x86_64/libepee.a lib-arm64/libepee.a -output lib-ios/libepee.a
#lipo -create lib-armv7/libeasylogging.a lib-x86_64/libeasylogging.a lib-armv8-a/libeasylogging.a -output lib-ios/libeasylogging.a
#popd

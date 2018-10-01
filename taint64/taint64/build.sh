#!/bin/bash
#find location of this build script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

#exit on error from any command
set -e

if [ -z "${build_parallel}" ]; then
    num_cores=`cat /proc/cpuinfo | grep processor | wc -l`
    build_parallel=$(($num_cores+1))
fi

if [ -z "${CC}" ]; then
  export CC=/usr/bin/gcc-5
fi
if [ -z "${CXX}" ]; then
  export CXX=/usr/bin/g++-5
fi

pushd ${DIR}
[ -f aclocal.m4 ] && rm -f aclocal.m4
[ -d m4 ] || libtoolize
autoreconf --verbose --force --install
[ -d build ] && rm -r build
SUBDIRS="build/8 build/16 build/32 build/64"
mkdir -p ${SUBDIRS}

for subdir in ${SUBDIRS}
do
    pushd $subdir
    ../../configure --prefix=/usr/local/taint
    size=$(basename ${subdir})
    make TAG_SIZE=${size} -j${build_parallel}
    cp tools/.libs/libdft_dta64.so ../libdft_dta64_u${size}.so
    popd
done
popd

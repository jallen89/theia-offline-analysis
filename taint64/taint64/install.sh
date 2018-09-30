#!/bin/bash
#find location of this build script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

#exit on error from any command
set -e

LIBS='build/libdft_dta64_u*.so'
pushd ${DIR}
[ $(ls -1 ${LIBS} | wc -l) -eq 4 ] \
|| { echo libraries are missing or not built yet;
    false; }
cp -v build/libdft_dta64_u*.so /usr/local/taint/lib
popd

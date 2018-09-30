#!/bin/bash
pin_prefix=/usr/local/taint
pin_version=$( \
    ${pin_prefix}/bin/pinbin -version \
    | grep ^Pin \
    | cut -d\  -f2
    )
set -e
[ x"${pin_version}" != x ]
fpm \
-s dir \
-t deb \
--deb-no-default-config-files \
-n theia-taint-pin \
-v ${pin_version} \
--depends libstdc++6 \
${pin_prefix}/bin/pinbin=/usr/local/taint/bin/ \
${pin_prefix}/lib/libpin.a=/usr/local/taint/lib/ \
${pin_prefix}/lib/libpinapp.a=/usr/local/taint/lib/ \
${pin_prefix}/lib/libpinjitprofiling.so=/usr/local/taint/lib/ \
${pin_prefix}/lib/libpinvm.a=/usr/local/taint/lib/ \
${pin_prefix}/lib/libsapin.a=/usr/local/taint/lib/ \
${pin_prefix}/lib/libxed.a=/usr/local/taint/lib/ \
${pin_prefix}/include/pin=/usr/local/taint/include/

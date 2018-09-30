#!/bin/bash
avrocpp_version=$( \
    ls /usr/local/taint/lib/libavrocpp.so.*.*.* \
    | xargs basename \
    | cut -c 15- \
    )
avrocpp_prefix=/usr/local/taint
avrocpp_libs=$( \
    find ${avrocpp_prefix}/lib -name 'libavrocpp*' -print0 \
    | xargs -0 -n1 -I% echo %=/usr/local/taint/lib/ \
    )
fpm \
-s dir \
-t deb \
--deb-no-default-config-files \
-n theia-taint-avrocpp \
-v ${avrocpp_version} \
--provides libavrocpp.so \
--depends libboost-iostreams-mt.so.1.54.0 \
--depends libstdc++6 \
${avrocpp_libs} \
${avrocpp_prefix}/include/avro=/usr/local/taint/include/

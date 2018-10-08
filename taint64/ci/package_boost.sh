#!/bin/bash
boost_version=$( \
    ls /usr/local/taint/lib/libboost_system-mt.so.*.*.* \
    | xargs basename \
    | cut -c 23- \
    | tr _ - \
    )
boost_prefix=/usr/local/taint
boost_libs=$( \
    find ${boost_prefix}/lib -name 'libboost*' -print0 \
    | xargs -0 -n1 -I% echo %=/usr/local/taint/lib/ \
    )
fpm \
-s dir \
-t deb \
--deb-no-default-config-files \
-n theia-taint-boost \
-v ${boost_version} \
${boost_libs} \
${boost_prefix}/include/boost=/usr/local/taint/include/

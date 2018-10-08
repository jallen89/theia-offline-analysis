#!/bin/bash
api_version=$( \
    ls /usr/local/taint/lib/libtcta3cpp.so.*.* \
    | xargs basename \
    | cut -c 16- \
    )
api_prefix=/usr/local/taint
api_libs=$( \
    find ${api_prefix}/lib -name 'libtcta3cpp.so*' -print0 \
    | xargs -0 -n1 -I% echo %=/usr/local/taint/lib/ \
    )
fpm \
-s dir \
-t deb \
--deb-no-default-config-files \
-n theia-taint-api-bindings \
-v ${api_version} \
--depends libstdc++6 \
--depends 'theia-taint-schema = 19' \
${api_libs} \
${api_prefix}/include/libtcta3cpp=/usr/local/taint/include/

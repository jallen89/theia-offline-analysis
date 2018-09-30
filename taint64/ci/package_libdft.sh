#!/bin/bash
libdft_version=0$( \
    (cd ~/src/theia-ki-offline-analysis; git describe --long --tags)
    )
libdft_prefix=/usr/local/taint
libdft_libs=$( \
    find ${libdft_prefix}/lib -name 'libdft*.so*' -print0 \
    | xargs -0 -n1 -I% echo %=/usr/local/taint/lib/ \
    )
fpm \
-s dir \
-t deb \
--deb-no-default-config-files \
-n theia-libdft \
-v ${libdft_version} \
--depends libboost-system-mt.so.1.54.0 \
--depends libboost-log-mt.so.1.54.0 \
--depends libboost-iostreams-mt.so.1.54.0 \
--depends libdwarf.so \
--depends libelfg0 \
--depends libpqxx3-dev \
--depends libglib2.0-dev \
--depends libtcta3cpp.so.13.0.0 \
--depends libavrocpp.so \
--depends libstdc++6 \
--depends theia-taint-boost \
--depends 'theia-taint-librdkafka = 0.9.4' \
--depends 'theia-taint-schema = 19' \
--depends theia-taint-api-bindings \
--depends theia-taint-pin \
${libdft_libs}

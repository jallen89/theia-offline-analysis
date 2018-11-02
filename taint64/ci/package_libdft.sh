#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
libdft_version=0$( \
    (cd ~/src/theia-ki-offline-analysis; git describe --long --tags)
    )
libdft_prefix=${DIR}/../taint64/build
libdft_libs=$( \
    find ${libdft_prefix} -maxdepth 1 -name 'libdft*.so*' -print0 \
    | xargs -0 -n1 -I% echo %=/usr/local/taint/lib/ \
    )
fpm \
-s dir \
-t deb \
--deb-no-default-config-files \
-n theia-libdft \
-v ${libdft_version} \
--depends libdwarf.so \
--depends libelfg0 \
--depends libpqxx3-dev \
--depends libglib2.0-dev \
--depends theia-taint-avrocpp \
--depends libstdc++6 \
--depends 'theia-taint-boost = 1.54.0' \
--depends 'theia-taint-librdkafka = 0.9.4' \
--depends 'theia-taint-schema = 19' \
--depends 'theia-taint-api-bindings = 13.0.0' \
--depends theia-taint-pin \
${libdft_libs}

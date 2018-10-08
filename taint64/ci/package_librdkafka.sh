#!/bin/bash
SRC_PATH=~/src/librdkafka
librdkafka_version=$(
    (cd ${SRC_PATH}; git describe --tags \
    | sed -e 's/^v//')
    )
librdkafka_prefix=/usr/local/taint
fpm \
-s dir \
-t deb \
--deb-no-default-config-files \
-n theia-taint-librdkafka \
-v ${librdkafka_version} \
--depends libstdc++6 \
${librdkafka_prefix}/lib/librdkafka.so.1=/usr/local/taint/lib/ \
${librdkafka_prefix}/lib/librdkafka++.so.1=/usr/local/taint/lib/ \
${librdkafka_prefix}/lib/pkgconfig/rdkafka++.pc=/usr/local/taint/lib/pkgconfig/ \
${librdkafka_prefix}/include/librdkafka=/usr/local/taint/include/


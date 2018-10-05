#!/bin/bash
set -e

##librdkafka must be 0.11.x for python module to build
#git clone https://github.com/edenhill/librdkafka.git
#cd librdkafka
#git checkout -b 0.11.5
#rm -r *
#git reset --hard
#CFLAGS="-Wl,-rpath=/usr/local/lib" \
#CXXFLAGS="-Wl,-rpath=/usr/local/lib" \
#LDFLAGS="-Wl,-rpath=/usr/local/lib" \
#./configure --disable-dependency-tracking --prefix=/usr/local
#make
#make install

CFLAGS="-I/usr/local/include -Wl,-rpath=/usr/local/lib" \
CXXFLAGS="-I/usr/local/include -Wl,-rpath=/usr/local/lib" \
LDFLAGS="-L/usr/local/lib -Wl,-rpath=/usr/local/lib" \
fpm \
-s python \
-t deb \
confluent-kafka

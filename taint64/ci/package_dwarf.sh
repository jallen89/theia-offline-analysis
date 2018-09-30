#!/bin/bash
SRC_PATH=~/src/theia-ki-offline-analysis
dwarf_version=0$( \
    (cd ${SRC_PATH}; git describe --long --tags)
    )
dwarf_prefix=/usr/local/taint
fpm \
-s dir \
-t deb \
--deb-no-default-config-files \
-n theia-taint-dwarf \
-v ${dwarf_version} \
--provides libdwarf.so \
${dwarf_prefix}/lib/libdwarf.so=/usr/local/taint/lib/

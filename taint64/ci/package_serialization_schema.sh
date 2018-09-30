#!/bin/bash
#cpan XML::XPath
schema_version=$( \
    xpath -e '//CDM-VERSION/text()' ~/src/ta3-serialization-schema/pom.xml 2>/dev/null
    )
schema_prefix=/usr/local/taint
fpm \
-s dir \
-t deb \
--deb-no-default-config-files \
-n theia-taint-schema \
-v ${schema_version} \
${schema_prefix}/include/tc_schema=/usr/local/taint/include/

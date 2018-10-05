#!/bin/bash
set -e
avro_version='1.8.2-tupty'
deb_file="python-avro_${avro_version}_all.deb"
pushd ~/src
[ -d avro ] || git clone https://github.com/gth828r/avro
cd avro/lang/py
sed -i.backup -e "/version = / s/'.*'/'${avro_version}'/" setup.py
ant
python setup.py build
cd build
[ -f ${deb_file} ] && rm ${deb_file}
fpm -s python -t deb ./setup.py
mv ${deb_file} $(dirs +1)
popd

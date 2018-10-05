#!/bin/bash
set -e

get_pip_version() {
pip freeze | grep ^${1}= | sed -e "s#${1}=\+##"
}

#apt-get update && apt-get install openssl libssl-dev
#curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
#sudo python get-pip.py
#sudo pip install -I --upgrade setuptools
#sudo apt-get install rubygems ruby1.9.1 ruby1.9.1-dev
#sudo update-alternatives --set ruby /usr/bin/ruby1.9.1
#sudo update-alternatives --set gem /usr/bin/gem1.9.1
#sudo gem install --no-ri --no-rdoc fpm
#sudo apt-get install ant

avro_version='1.8.2-tupty'
#if [ x"$(get_pip_version avro)" != x"${avro_version}" ]; then
#    pip install --upgrade 'https://github.com/gth828r/avro/tarball/record-validation-fix#egg=avro-1.8.2-tupty&subdirectory=lang/py'
#fi
pushd ~/src
[ -d avro ] || git clone https://github.com/gth828r/avro
cd avro/lang/py
sed -i.backup -e "/version = / s/'.*'/'${avro_version}'/" setup.py
ant
python setup.py build
cd build
fpm -s python -t deb ./setup.py
cp python-avro_${avro_version}_all.deb ~/src

#quickavro has some stupid bug in its setup.py which prevents building
if [ x"$(get_pip_version quickavro)" == x ]; then
cat <<MESG
quickavro has a broken setup.py
manually clone the repo, and run:
#sudo apt-get install python-all-dev
#pip install stdeb
sed -i -e '/"pytest-runner",/d' setup.py
python setup.py --command-packages=stdeb.command debianize
dpkg-buildpackage -b
MESG
    exit 1
fi

if [ x"$(get_pip_version tc-bbn-py)" != x'19.20181001.0' ]; then
    pip install --upgrade 'git+ssh://git@git.tc.bbn.com/bbn/ta3-api-bindings-python.git@develop#egg=tc_bbn_py-19.20180907.0'
fi

fpm \
-s python \
-t deb \
--python-pip $(which pip) \
--verbose \
tc-bbn-py

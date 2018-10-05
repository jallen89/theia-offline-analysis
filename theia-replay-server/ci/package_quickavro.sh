#!/bin/bash
set -e

pushd ~/src
rm -f python-quickavro_*.deb
[ -d quickavro ] || git clone https://github.com/ChrisRx/quickavro.git
cd quickavro
rm -rf debian
git reset --hard
sed -i.backup \
	-e '/import pip/a \
try:\
	from pip import main as pip_main\
except:\
	from pip._internal import main as pip_main'\
	-e 's/pip\.main/pip_main/'\
	-e '/"pytest-runner",/d'\
	setup.py
#quickavro_version=$(python -c 'import setup; print(setup.get_version())')
python setup.py --command-packages=stdeb.command debianize --debian-version="STDEB"
sed -i -e 's/-STDEB//' debian/changelog
dpkg-buildpackage -b || dpkg-buildpackage -b -d
mv ../python-quickavro_*.deb $(dirs -l +1)
popd

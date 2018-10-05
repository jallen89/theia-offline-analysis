#!/bin/bash
set -e

pushd ~/src
[ -d ta3-api-bindings-python ] || git clone git+ssh://git@git.tc.bbn.com/bbn/ta3-api-bindings-python.git
cd ta3-api-bindings-python
rm -f python-tc-bbn-py_*.deb
fpm -s python -t deb ./setup.py
mv python-tc-bbn-py_*.deb $(dirs +1)
popd

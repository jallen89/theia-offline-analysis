#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
set -e

DEB='theia-offline-restapi_*.deb'
pushd ${DIR}/..
compgen -G "${DEB}" \
&& rm -f ${DEB}
fpm \
-s python \
-t deb \
-n theia-offline-restapi \
--depends gunicorn \
--depends theia-libdft \
./setup.py
compgen -G "${DEB}" \
&& mv -vf ${DEB} $(dirs -l +1)
popd

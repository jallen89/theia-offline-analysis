#!/bin/bash
set -e

fpm \
-s python \
-t deb \
-n theia-offline-restapi \
--depends gunicorn \
--depends theia-libdft \
./setup.py

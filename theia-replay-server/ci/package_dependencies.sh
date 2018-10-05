#!/bin/bash
set -e

DEPS='
marshmallow-mongoengine
neo4jrestclient
rq
redis
psycopg2
flask
marshmallow
ioctl-opt
click
requests
pymongo
flask-restful
mongoengine
futures
enum34
aniso8601
pytz
werkzeug
jinja2
itsdangerous
markupsafe
chardet
idna==2.7
urllib3==1.23
avro-json-serializer==0.5
'
name_prefix='python'
for pkg in ${DEPS}; do
[ -f ${name_prefix}-${pkg}_*.deb ] && rm ${name_prefix}-${pkg}_*.deb
fpm \
-s python \
-t deb \
--python-package-name-prefix ${name_prefix} \
--provides python-${pkg} \
${pkg}
done

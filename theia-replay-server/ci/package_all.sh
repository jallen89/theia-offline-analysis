#!/bin/bash
set -e
./package_avro.sh
./package_quickavro.sh
./package_tc_bbn.sh

./package_python_kafka.sh

./package_dependencies.sh

./package_restapi.sh

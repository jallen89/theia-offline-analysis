#!/bin/bash

# UUID for README.md
uuid=0100d00f-4407-9a00-0000-00009010ac5b

python theia-client.py forward-query "$uuid" "00000000000" "999999999"  "15"

#!/bin/bash
uuid=440c7b00-0000-0000-0000-000000000020
#uuid=0100d00f-7630-3400-0000-0000221b7909
uuid=440c00a0-22c6-b07f-0000-000000000050

echo "Sending replay 1."
python theia-client.py backward-query "$uuid" "000" "1234"  "2" &

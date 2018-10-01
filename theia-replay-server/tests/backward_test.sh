#!/bin/bash
# UUID for out.tmp
uuid="0100d00f-6007-9a00-0000-00007413ac5b"
echo "Sending replay 1."
theia-client backward-query "$uuid" "000" "99999999999999999"  "2" &

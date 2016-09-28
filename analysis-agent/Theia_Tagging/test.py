#!/usr/bin/python

import sys
import time
import subprocess

path = sys.argv[1]
p = subprocess.Popen(['/home/yang/theia-es/test/resume', 
                      path,'-p', '--pthread', 
                      '/home/yang/theia-es/eglibc-2.15/prefix/lib'])
print p.pid
time.sleep(5)
q = subprocess.Popen(['/home/yang/Downloads/pin-2.13/pin', '-pid', str(p.pid), '-t',
                      '/home/yang/theia-es/pin_tools/obj-ia32/print_instructions.so'])



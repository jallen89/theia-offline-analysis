#!/usr/bin/python

import sys
import time
import subprocess

path = sys.argv[1]
print '(' + sys.argv[0] + ')'
print '(' + sys.argv[1] + ')'
print '(' + sys.argv[2] + ')'
print '(' + path + ')'
p = subprocess.Popen(['/home/yang/omniplay/test/resume', 
                      path,'-p', '--pthread', 
                      '/home/yang/omniplay/eglibc-2.15/prefix/lib'])
print p.pid
time.sleep(5)
#q = subprocess.Popen(['/home/yang/Downloads/pin-2.13/pin', '-pid', str(p.pid), '-t',
#                      '/home/yang/omniplay/pin_tools/obj-ia32/print_instructions.so'])
q = subprocess.Popen(['/home/yang/omniplay/pin_tools/dtracker/pin/pin', '-pid', str(p.pid), '-t',
                      '/home/yang/omniplay/pin_tools/dtracker/obj-ia32/dtracker.so'])



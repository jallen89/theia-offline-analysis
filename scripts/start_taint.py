#!/usr/bin/python

import sys
import time
import subprocess
import os.path

if len(sys.argv) != 9:
  print "Input argument error."
  exit()

print '(command=' + sys.argv[0] + ')'
print '(query type=' + sys.argv[1] + ')'
print '(query id=' + sys.argv[2] + ')'
print '(subject uuid=' + sys.argv[3] + ')'
print '(local principal=' + sys.argv[4] + ')'
print '(replay path=' + sys.argv[5] + ')'
print '(kafka address:ipport=' + sys.argv[6] + ')'
print '(kafka topic=' + sys.argv[7] + ')'
print '(tag counter=' + sys.argv[8] + ')'

query_type = sys.argv[1]
query_id = sys.argv[2]
subject_uuid = sys.argv[3]
local_principal = sys.argv[4]
path = sys.argv[5]
kafka_ipport = sys.argv[6]
kafka_topic = sys.argv[7]
tag_counter = sys.argv[8]


p = subprocess.Popen(['/home/theia/theia-es/test/theia_replay', 
                      path,'-p', '--pthread', 
                      '/home/theia/theia-es/eglibc-2.15/prefix/lib'])

print p.pid
time.sleep(5)

# currently not support backward
q = subprocess.Popen(['/home/theia/theia-es/libdft64/pin-2.13/intel64/bin/pinbin', '-pid',
  str(p.pid), '-t', '/home/theia/theia-es/libdft64/libdft64/build/tools/.libs/libdft-dta64.so', 
  '-publish_to_kafka', 'true', '-kafka_server', kafka_ipport, '-kafka_topic', kafka_topic, 
  '-create_avro_file', 'true', '-query_id', query_id, '-subject_uuid', subject_uuid, '-local_principal', local_principal, '-tag_counter', tag_counter])
p.communicate()
q.wait();

#!/usr/bin/python

import sys
import time
import subprocess

if len(sys.argv) != 9:
  print "Input argument error."
  exit()

query_type = sys.argv[1]
query_id = sys.argv[2]
path = sys.argv[3]
kafka_ipport = sys.argv[4]
kafka_topic = sys.argv[5]
kafka_binfile = sys.argv[6]
source_id = sys.argv[7]
sink_id = sys.argv[8]

print '(' + sys.argv[0] + ')'
print '(' + sys.argv[1] + ')'
print '(' + sys.argv[2] + ')'
print '(' + sys.argv[3] + ')'
print '(' + sys.argv[4] + ')'
print '(' + sys.argv[5] + ')'
print '(' + sys.argv[6] + ')'
print '(' + sys.argv[7] + ')'
print '(' + sys.argv[8] + ')'


p = subprocess.Popen(['/home/theia/theia-es/test/resume', 
                      path,'-p', '--pthread', 
                      '/home/theia/theia-es/eglibc-2.15/prefix/lib'])

print p.pid
time.sleep(5)

# currently not support backward
if query_type == "backward":
  q = subprocess.Popen(['/home/theia/theia-es/pin_tools/dtracker/pin/pin', '-pid',
    str(p.pid), '-t', '/home/theia/theia-es/pin_tools/dtracker/obj-ia32/dtracker.so', 
    '-no_neo4j', 'true', '-tag_count_file', '/home/theia/tags.txt', '-publish_to_kafka', 
    'true', '-kafka_server', kafka_ipport, '-kafka_topic', kafka_topic, 
    '-create_avro_file', 'true', '-avro_file', kafka_binfile, '-query_id', query_id, 
    '-use_sink_id', 'true', '-sink_id', sink_id])

elif query_type == "forward":
  q = subprocess.Popen(['/home/theia/theia-es/pin_tools/dtracker/pin/pin', '-pid',
    str(p.pid), '-t', '/home/theia/theia-es/pin_tools/dtracker/obj-ia32/dtracker.so', 
    '-no_neo4j', 'true', '-tag_count_file', '/home/theia/tags.txt', '-publish_to_kafka', 
    'true', '-kafka_server', kafka_ipport, '-kafka_topic', kafka_topic, 
    '-create_avro_file', 'true', '-avro_file', kafka_binfile, '-query_id', query_id, 
    '-use_source_id', 'true', '-source_id', source_id])

elif query_type == "point-to-point":
  q = subprocess.Popen(['/home/theia/theia-es/pin_tools/dtracker/pin/pin', '-pid',
    str(p.pid), '-t', '/home/theia/theia-es/pin_tools/dtracker/obj-ia32/dtracker.so', 
    '-no_neo4j', 'true', '-tag_count_file', '/home/theia/tags.txt', '-publish_to_kafka', 
    'true', '-kafka_server', kafka_ipport, '-kafka_topic', kafka_topic, 
    '-create_avro_file', 'true', '-avro_file', kafka_binfile, '-query_id', query_id, 
    '-use_source_id', 'true', '-source_id', source_id, '-use_sink_id', 'true', '-sink_id', sink_id])


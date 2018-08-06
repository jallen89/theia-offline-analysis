#!/usr/bin/python

import sys
import time
import subprocess

path = sys.argv[1]
print '(' + sys.argv[0] + ')'
print '(' + sys.argv[1] + ')'
print '(' + sys.argv[2] + ')'
print '(' + path + ')'
p = subprocess.Popen(['/home/theia/theia-es/test/resume', 
                      path,'-p', '--pthread', 
                      '/home/theia/theia-es/eglibc-2.15/prefix/lib'])
print p.pid
time.sleep(5)

q = subprocess.Popen(['/home/theia/theia-es/pin_tools/dtracker/pin/pin', '-pid',
										  str(p.pid), '-t', '/home/theia/theia-es/pin_tools/dtracker/obj-ia32/dtracker.so', 
											'-no_neo4j', 'true', '-tag_count_file', '/home/theia/tags.txt', '-publish_to_kafka', 
											'true', '-kafka_server', '10.0.50.19:9092', '-kafka_topic', 'ta1-theia-qr', 
											'-create_avro_file', 'true', '-avro_file', '/home/theia/1_tags.bin', '-query_id', '1', 
											'-use_source_id', 'true', '-source_id', '700000269']);



#!/usr/bin/python

import os, sys
import subprocess
import psycopg2

def get_immediate_subdirectories(a_dir):
  return [name for name in os.listdir(a_dir)
  if os.path.isdir(os.path.join(a_dir, name))]

def read_ckpt(rec):
  p = subprocess.Popen(['/home/theia/theia-es/test/parseckpt', 
    '/data/replay_logdb/'+rec], stdout=subprocess.PIPE) 
  output = p.communicate()[0]

  pid_start = output.find('record pid:') + 12
  pid_end = output.find('\n')
  pid = output[pid_start:pid_end]
  cmdline_start = output.find('record filename:') + 17
  cmdline_end = output[cmdline_start:].find('\n')
  cmdline = output[cmdline_start:cmdline_start+cmdline_end]
  print pid + "," + cmdline
  global cur
  cur.execute("INSERT INTO rec_index (procname, dir) SELECT %s, %s \
							WHERE NOT EXISTS (SELECT 0 FROM rec_index where procname = %s);"\
							, (pid+cmdline, '/data/replay_logdb/'+rec, pid+cmdline))

conn = psycopg2.connect("host=10.0.6.209 dbname=theia1 user=theia password=darpatheia1")
#conn = psycopg2.connect("host=143.215.130.137 dbname=yang user=yang password=darpatheia1")
cur = conn.cursor()
cur.execute("CREATE TABLE IF NOT EXISTS rec_index (procname varchar(100),dir varchar(100));")
cur.execute("DELETE FROM rec_index;")

for rec in get_immediate_subdirectories('/data/replay_logdb'):
  read_ckpt(rec)
conn.commit()
cur.close()
conn.close()

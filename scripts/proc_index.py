#!/usr/bin/python

import os, sys
import subprocess
import psycopg2
import os.path

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
  exe_pos = cmdline.rfind('/')
  if exe_pos == -1:
    exe_name = cmdline;
  else:
    exe_name = cmdline[exe_pos+1:]
  print pid + "," + exe_name 
  global cur
  cur.execute("INSERT INTO rec_index (procname, dir) SELECT %s, %s \
							WHERE NOT EXISTS (SELECT 0 FROM rec_index where procname = %s);"\
							, (pid+exe_name, '/data/replay_logdb/'+rec, pid+exe_name))

conn = psycopg2.connect("host=127.0.0.1 dbname=theia.db user=theia password=darpatheia1")
cur = conn.cursor()
cur.execute("CREATE TABLE IF NOT EXISTS rec_index (procname varchar(100),dir varchar(100));")
cur.execute("DELETE FROM rec_index;")

for rec in get_immediate_subdirectories('/data/replay_logdb'):
  read_ckpt(rec)
conn.commit()
cur.close()
conn.close()

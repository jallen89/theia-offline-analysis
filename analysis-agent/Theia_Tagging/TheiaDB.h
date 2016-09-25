#ifndef __THEIADB_H__
#define __THEIADB_H__

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <pqxx/pqxx> 

#include "TheiaTagging.h"

using namespace std;
using namespace pqxx;


string get_replay_path(int pid, string cmdline);

void insert_entry_postgres(int pid, string cmdline, SyscallType syscall,
    int64_t timestamp, string file_name, u_long uuid, SyscallStruct syscall_struct);

void query_entry_postgres(Proc_itlv_grp_type &proc_itlvgrp_map, 
  int64_t start_time, int64_t end_time, string obj_out);

//extern Theia_db_postgres theia_db;

#endif

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

void insert_path_uuid_postgres(string path, u_long uuid);
u_long query_uuid_postgres(string path);

void insert_entry_postgres(int pid, string cmdline, SyscallType syscall,
    int64_t timestamp, string file_name, u_long uuid, SyscallStruct syscall_struct);

void query_entry_postgres(Proc_itlv_grp_type &proc_itlvgrp_map, 
  int64_t start_time, int64_t end_time, string obj_out);

long query_file_tagging_postgres(u_long f_uuid, off_t offset, ssize_t* p_size);
void insert_file_tagging_postgres(u_long f_uuid, off_t offset, ssize_t size, u_long tag_uuid);
//extern Theia_db_postgres theia_db;

#endif

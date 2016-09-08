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


void insert_entry_postgres(int pid, string cmdline, SyscallType syscall,
    int64_t timestamp, string file_name, SyscallStruct syscall_struct);


//extern Theia_db_postgres theia_db;

#endif

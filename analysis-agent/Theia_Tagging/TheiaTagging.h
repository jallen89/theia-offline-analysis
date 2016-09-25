#ifndef __THEIATAGGING_H__
#define __THEIATAGGING_H__

#include <string>
#include <vector>
#include <map>

using namespace std;

typedef unsigned long u_long;

enum SyscallType {
  READ_SYSCALL = 0,
  WRITE_SYSCALL,
  MMAP_SYSCALL,
  SEND_SYSCALL,
  RECEIVE_SYSCALL
};

struct SyscallStruct {
  SyscallType syscall;
  int64_t clock;
  int64_t timestamp;
  string file_name;
	u_long uuid;
};

class ProcItlvGrp {
  public:
    int pid;
    string cmdline;
    vector < SyscallStruct > inbound_events;
    vector < SyscallStruct > outbound_events;
};

typedef map <string, ProcItlvGrp> Proc_itlv_grp_type;

void handle_itlv(int pid, string cmdline, SyscallType syscall,
	int64_t timestamp, string file_name, u_long uuid);



void update_procItLvGrp(Proc_itlv_grp_type& proc_itlvgrp_map, 
	int pid, string cmdline, SyscallType syscall,
	int64_t timestamp, string file_name, u_long uuid);

#endif

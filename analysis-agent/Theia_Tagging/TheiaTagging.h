#ifndef __THEIATAGGING_H__
#define __THEIATAGGING_H__

#include <string>
#include <vector>
#include <map>

using namespace std;

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
};

class ProcItlvGrp {
  public:
    int pid;
    string cmdline;
    map <int64_t/*timestamp*/, SyscallStruct > inbound_events;
    map <int64_t/*timestamp*/, SyscallStruct > outbound_events;
};

void handle_itlv_read(int pid, string cmdline, SyscallType syscall,
int64_t timestamp, string file_name);

#endif

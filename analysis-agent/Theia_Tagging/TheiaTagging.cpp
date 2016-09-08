#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <pqxx/pqxx> 

#include "TheiaTagging.h"
#include "TheiaDB.h"

using namespace std;

map <string, ProcItlvGrp> proc_itlvgrp_map;

//extern Theia_db_postgres theia_db;

bool is_inbound_event(SyscallType syscall) {
  if(syscall == READ_SYSCALL || 
    syscall == RECEIVE_SYSCALL || 
    syscall == MMAP_SYSCALL) {
    return true;
  }
  else
    return false;

}

void update_procItLvGrp(int pid, string cmdline, SyscallType syscall,
int64_t timestamp, string file_name) {
  stringstream buff;
  buff << pid << cmdline;
  string procname = buff.str();

  if(proc_itlvgrp_map.find(procname) != proc_itlvgrp_map.end()) {
    auto& proc_grp = proc_itlvgrp_map[procname];
    
    SyscallStruct syscall_struct;
    syscall_struct.syscall = syscall;
    //Yang: will fix by intercepting the recording from kernel.
    syscall_struct.clock = 0; 
    syscall_struct.timestamp = timestamp;
    syscall_struct.file_name = file_name;

    if(is_inbound_event(syscall)) {
      auto& inb_events = proc_grp.inbound_events;
      inb_events[timestamp] = syscall_struct;
    }
    else {
      auto& out_events = proc_grp.outbound_events;
      out_events[timestamp] = syscall_struct;
    }
  }
  else {
    ProcItlvGrp proc_grp;
    proc_grp.pid = pid;
    proc_grp.cmdline = cmdline;
    
    SyscallStruct syscall_struct;
    syscall_struct.syscall = syscall;
    //Yang: will fix by intercepting the recording from kernel.
    syscall_struct.clock = 0; 
    syscall_struct.timestamp = timestamp;
    syscall_struct.file_name = file_name;
    if(is_inbound_event(syscall)) {
      proc_grp.inbound_events[timestamp] = syscall_struct;
    }
    else {
      proc_grp.outbound_events[timestamp] = syscall_struct;
    }
    proc_itlvgrp_map[procname] = proc_grp;
  }
}

void handle_itlv_read(int pid, string cmdline, SyscallType syscall,
int64_t timestamp, string file_name) {

  //Yang: first, we check whether it is an outbound event
  if(!is_inbound_event(syscall)) {
    stringstream buff;
    buff << pid << cmdline;
    string procname = buff.str();
    
    if(proc_itlvgrp_map.find(procname) != proc_itlvgrp_map.end()) {
      auto proc_grp = proc_itlvgrp_map[procname];
      auto inb_evts = proc_grp.inbound_events;
      for(auto it=inb_evts.begin();it!=inb_evts.end();it++) {
        insert_entry_postgres(pid, cmdline, syscall, timestamp, 
          file_name, it->second);
      }
    }
  
  }

  //Yang: we then update the ProcItlvGrp.
  update_procItLvGrp(pid, cmdline, syscall, timestamp, file_name);
}



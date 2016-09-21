#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <pqxx/pqxx> 

#include "TheiaTagging.h"
#include "TheiaDB.h"

using namespace std;
Proc_itlv_grp_type glb_proc_itlvgrp_map;

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

void update_procItLvGrp(Proc_itlv_grp_type & proc_itlvgrp_map, 
	int pid, string cmdline, SyscallType syscall,
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
      inb_events.push_back(syscall_struct);
    }
    else {
      auto& out_events = proc_grp.outbound_events;
      out_events.push_back(syscall_struct);
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
      proc_grp.inbound_events.push_back(syscall_struct);
    }
    else {
      proc_grp.outbound_events[timestamp] = syscall_struct;
    }
    proc_itlvgrp_map[procname] = proc_grp;
  }

  for(auto it=proc_itlvgrp_map.begin();it!=proc_itlvgrp_map.end();it++){
    auto name = it->first;
    if(name.find("simp_file") != string::npos) {
      auto grp = (it->second);
      cout << name << "\n";
      for(auto itt=grp.inbound_events.begin();itt!=grp.inbound_events.end();itt++) {
        cout << "inb:" << (*itt).syscall << "," << (*itt).file_name << "\n";
      }
      for(auto itt=grp.outbound_events.begin();itt!=grp.outbound_events.end();itt++) {
        cout << "outb:" << (*itt).syscall << "," << (*itt).file_name << "\n";
      }

    }
  }
}

void handle_itlv(int pid, string cmdline, SyscallType syscall,
int64_t timestamp, string file_name) {

  //Yang: first, we check whether it is an outbound event
  if(!is_inbound_event(syscall)) {
    stringstream buff;
    buff << pid << cmdline;
    string procname = buff.str();
    if(glb_proc_itlvgrp_map.find(procname) != glb_proc_itlvgrp_map.end()) {
      auto proc_grp = glb_proc_itlvgrp_map[procname];
      auto inb_evts = proc_grp.inbound_events;
      for(auto it=inb_evts.begin();it!=inb_evts.end();it++) {
        insert_entry_postgres(pid, cmdline, syscall, timestamp, 
          file_name, *it);
      }
    }
  
  }

  //Yang: we then update the ProcItlvGrp.
  update_procItLvGrp(glb_proc_itlvgrp_map, pid, cmdline, syscall, timestamp, file_name);
}



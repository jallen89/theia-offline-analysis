#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <pqxx/pqxx> 

#include "TheiaTagging.h"
#include "TheiaDB.h"
#include "queue_ahg.h"
//#include <stxxl/queue>


Queue<struct ItlvStruct> itlv_queue;
//typedef stxxl::queue<struct ItlvStruct> itlv_queue_type;
//itlv_queue_type itlv_queue;

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

bool is_outbound_event(SyscallType syscall) {
  if(syscall == WRITE_SYSCALL || 
    syscall == SEND_SYSCALL || 
    syscall == MMAP_SYSCALL) {
    return true;
  }
  else
    return false;

}

void update_procItLvGrp(Proc_itlv_grp_type & proc_itlvgrp_map, 
	int pid, string cmdline, SyscallType syscall,
	int64_t timestamp, string file_name, u_long uuid, uint32_t version) {

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
    syscall_struct.uuid = uuid;
    syscall_struct.version = version;

//Yang: it seems we will get the the file name and uuid from TA2's query.
		insert_path_uuid_postgres(file_name, version, uuid);

    if(is_inbound_event(syscall)) {
      auto& inb_events = proc_grp.inbound_events;
      inb_events.push_back(syscall_struct);
    }
    else {
      auto& out_events = proc_grp.outbound_events;
      out_events.push_back(syscall_struct);
    }

    //insert to syscall table
    insert_syscall_entry(pid, cmdline, syscall_struct);

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
    syscall_struct.uuid = uuid;
    syscall_struct.version = version;
//Yang: it seems we will get the the file name and uuid from TA2's query.
		insert_path_uuid_postgres(file_name, version, uuid);

    if(is_inbound_event(syscall)) {
      proc_grp.inbound_events.push_back(syscall_struct);
    }
    else {
      proc_grp.outbound_events.push_back(syscall_struct);
    }
    proc_itlvgrp_map[procname] = proc_grp;


    //insert to syscall table
    insert_syscall_entry(pid, cmdline, syscall_struct);

  }

#ifdef THEIA_DEBUG
  for(auto it=proc_itlvgrp_map.begin();it!=proc_itlvgrp_map.end();it++){
    auto name = it->first;
    if(name.find("2119/bin/ls") != string::npos) {
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
#endif
}

void execute_handle_itlv() {
  //if(!itlv_queue.empty()){
//    auto itlv = itlv_queue.front();
    auto itlv = itlv_queue.pop();
    //Yang: we then update the ProcItlvGrp.
    update_procItLvGrp(glb_proc_itlvgrp_map, itlv.pid, itlv.cmdline, 
        itlv.syscall, itlv.timestamp, itlv.file_name, itlv.uuid, itlv.version);
 // }
  //Yang: first, we check whether it is an outbound event
//  if(!is_inbound_event(itlv.syscall)) {
//    stringstream buff;
//    buff << itlv.pid << itlv.cmdline;
//    string procname = buff.str();
//    if(glb_proc_itlvgrp_map.find(procname) != glb_proc_itlvgrp_map.end()) {
//      auto proc_grp = glb_proc_itlvgrp_map[procname];
//      auto inb_evts = proc_grp.inbound_events;
//      for(auto it=inb_evts.begin();it!=inb_evts.end();it++) {
//        insert_entry_postgres(itlv.pid, itlv.cmdline, itlv.syscall, itlv.timestamp, 
//          itlv.file_name, itlv.uuid, *it);
//				insert_path_uuid_postgres(itlv.file_name, itlv.uuid);
//      }
//    }
//  }
//


}

void handle_itlv(int pid, string cmdline, SyscallType syscall,
	int64_t timestamp, string file_name, u_long uuid, uint32_t version) {
  struct ItlvStruct itlv;
  itlv.pid = pid;
  itlv.cmdline = cmdline;
  itlv.syscall = syscall;
  itlv.timestamp = timestamp;
  itlv.file_name = file_name;
  itlv.uuid = uuid;
  itlv.version = version;

  itlv_queue.push(itlv);    
}



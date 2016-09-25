//build:
// g++ -g -std=c++0x -Wall -o db_test db_tests.cpp TheiaDB.cpp TheiaTagging.cpp -lpqxx -lpq

#include "TheiaDB.h"                                                             
#include "TheiaTagging.h"                                                             
#include <string>
#include <iostream>

using namespace std;
                                                                                 
int main(int argc, char* argv[]) {
	string host_name;
	int64_t start_time, end_time;

	if (argc != 4) {
		printf("Invalid inputs. \n./start_track host start_time end_time");
		return 1;
	}

	host_name = string(argv[1]);
	start_time = atoll(argv[2]);
	end_time = atoll(argv[3]);
	if(start_time >= end_time) {
		printf("Invalid timestamps.\n");
		return 1;
	}

  Proc_itlv_grp_type proc_itlvgrp_map;
  query_entry_postgres(proc_itlvgrp_map, start_time, end_time, host_name);

  for (auto it=proc_itlvgrp_map.begin(); it != proc_itlvgrp_map.end(); ++it) {
    auto itlv_grp = it->second;

    string replay_path = get_replay_path(itlv_grp.pid, itlv_grp.cmdline);
    cout << replay_path << "\n";
    if(replay_path == "ERROR") {
      cout << "replay_path error: " << replay_path << "\n";
    }

//    start_tracking(replay_path, 
//                   itlv_grp.inbound_events.front().clock, 
//                   itlv_grp.outbound_events.back().clock);

    // use the syscall_struct for dtracker input;
  }
	/*replay and data tracking starts here*/
//	for() {
//		//replay
//	}
  return 0;                                                                      
}

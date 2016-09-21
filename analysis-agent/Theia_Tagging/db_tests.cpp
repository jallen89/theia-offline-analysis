//build:
// g++ -g -std=c++0x -Wall -o db_test db_tests.cpp TheiaDB.cpp -lpqxx -lpq

#include "TheiaDB.h"                                                             
#include "TheiaTagging.h"                                                             
#include <string>
#include <iostream>

using namespace std;
                                                                                 
int main(int argc, char* argv[]) {
	string host_name;
	int64_t start_time, end_time;
	cout << "int64_t " << sizeof(int64_t) << ", long long " << sizeof(long long int) << "\n";

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

	/*replay and data tracking starts here*/
//	for() {
//		//replay
//	}
  return 0;                                                                      
}

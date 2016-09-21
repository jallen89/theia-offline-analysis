#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <pqxx/pqxx> 

#include "TheiaDB.h"
#include "TheiaTagging.h"

using namespace std;
using namespace pqxx;

void insert_entry_postgres(int pid, string cmdline, SyscallType syscall,
    int64_t timestamp, string file_name, SyscallStruct syscall_struct) {

  try{
    static connection C("dbname=yang user=yang password=yang \
        hostaddr=127.0.0.1 port=5432");
    if (C.is_open()) {
      cout << "Opened database successfully: " << C.dbname() << endl;
    } else {
      cout << "Can't open database" << endl;
      return;
    }

    stringstream buff;
    /* Create SQL statement */
    buff << "INSERT INTO CLST (pid,cmdline,syscall_src,syscall_sink,syscall_src_T,syscall_sink_T,obj_in,obj_out) " << "VALUES (" << pid << ",'" << cmdline << "'," <<
      syscall_struct.syscall << "," << syscall << "," << syscall_struct.timestamp << "," 
      << timestamp << ",'" << syscall_struct.file_name << "','" << file_name << "');";

    cout << buff.str() << "\n";

    /* Create a transactional object. */
    work W(C);

    /* Execute SQL query */
    W.exec( buff.str().c_str() );
    W.commit();

  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return;
  }

}

void query_entry_postgres(Proc_itlv_grp_type &proc_itlvgrp_map, 
  int64_t start_time, int64_t end_time, string obj_out) {
                                                                                 
  try{                                                                           
    static connection C("dbname=yang user=yang password=yang \
        hostaddr=127.0.0.1 port=5432");                                          
    if (C.is_open()) {                                                           
      cout << "Opened database successfully: " << C.dbname() << endl;            
    } else {                                                                     
      cout << "Can't open database" << endl;                                     
      return;                                                                    
    }                                                                            
                                                                                 
    stringstream buff;                                                           
    /* Create SQL statement */                                                   
    buff << "SELECT * FROM CLST WHERE" << " obj_out = '" << obj_out << "' AND " <<
      "syscall_sink_T BETWEEN " << start_time << " AND " << end_time << ";";        
                                                                                 
    cout << buff.str() << "\n";

    /* Create a non-transactional object. */
    nontransaction N(C);

    /* Execute SQL query */
    result R( N.exec(buff.str().c_str()));

    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
			auto pid = c[0].as<int>(); 
			auto cmdline = c[1].as<string>();
			auto syscall_src = c[2].as<string>();
			auto syscall_sink = c[3].as<string>();
			auto syscall_src_T = c[4].as<int64_t>();
			auto syscall_sink_T = c[5].as<int64_t>();
			auto obj_in = c[6].as<string>();
			auto obj_out = c[7].as<string>();

#ifdef THEIA_DEBUG
      cout << "pid: " << c[0].as<int>() 
           << ", cmdline: " << c[1].as<string>() 
           << ", syscall_src: " << c[2].as<string>() 
           << ", syscall_sink: " << c[3].as<string>() 
           << ", syscall_src_T: " << c[4].as<long>() 
           << ", syscall_sink_T: " << c[5].as<long>() 
           << ", obj_in: " << c[6].as<string>() 
           << ", obj_out: " << c[7].as<string>() << "\n";                                                 
#endif
			/*we will return the merged interleavings in every process group*/
			update_procItLvGrp(proc_itlvgrp_map, pid, cmdline, 
				SyscallType(stoi(syscall_src)), syscall_src_T, obj_in);
			update_procItLvGrp(proc_itlvgrp_map, pid, cmdline, 
				SyscallType(stoi(syscall_sink)), syscall_sink_T, obj_out);
    }
		return;
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return; 
  }

}

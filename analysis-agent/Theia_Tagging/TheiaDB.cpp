#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <pqxx/pqxx> 

#include "TheiaDB.h"
#include "TheiaTagging.h"

using namespace std;
using namespace pqxx;

string get_replay_path(int pid, string cmdline) {
  try{
    static connection C("dbname=rec_index user=yang password=yang \
        hostaddr=127.0.0.1 port=5432");
    if (C.is_open()) {
      cout << "Opened database successfully: " << C.dbname() << endl;
    } else {
      cout << "Can't open database" << endl;
      return "ERROR";
    }

    stringstream buff;
    /* Create SQL statement */
    buff << "SELECT dir FROM rec_index WHERE procname = '" <<  pid  << cmdline << "';";

#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    /* Create a non-transactional object. */
    nontransaction N(C);

    /* Execute SQL query */
    result R( N.exec(buff.str().c_str()));

    if (R.size() != 1) {
      cout << "More than one replay group found, what the chance! (" << pid << cmdline << "\n";
    }
    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
			auto path = c[0].as<string>(); 
      return path; //the first found path is returned.
    }
    return "ERROR";
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return "ERROR"; 
  }
}

void insert_entry_postgres(int pid, string cmdline, SyscallType syscall,
    int64_t timestamp, string file_name, u_long out_uuid, SyscallStruct syscall_struct) {

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
    buff << "INSERT INTO CLST (pid,cmdline,syscall_src,syscall_sink,\
      syscall_src_T,syscall_sink_T,obj_in,obj_out,obj_in_uuid,obj_out_uuid) " 
			<< "VALUES (" << pid << ",'" << cmdline << "'," << syscall_struct.syscall << "," 
      << syscall << "," << syscall_struct.timestamp << "," << timestamp 
      << ",'" << syscall_struct.file_name << "','" << file_name << "'," 
			<< syscall_struct.uuid << "," << out_uuid << ");";

#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

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
    buff << "SELECT * FROM CLST WHERE" << " obj_out = '" << obj_out << "' AND "
      << "syscall_sink_T BETWEEN " << start_time << " AND " << end_time << ";";        
                                                                                 
#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    /* Create a non-transactional object. */
    nontransaction N(C);

    /* Execute SQL query */
    result R( N.exec(buff.str().c_str()));

    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
			auto pid = c[1].as<int>(); 
			auto cmdline = c[2].as<string>();
			auto syscall_src = c[3].as<string>();
			auto syscall_sink = c[4].as<string>();
			auto syscall_src_T = c[5].as<int64_t>();
			auto syscall_sink_T = c[6].as<int64_t>();
			auto obj_in = c[7].as<string>();
			auto obj_out = c[8].as<string>();
			auto in_uuid = c[9].as<u_long>();
			auto out_uuid = c[10].as<u_long>();
      

#ifdef THEIA_DEBUG
      cout << "pid: " << c[1].as<int>() 
           << ", cmdline: " << c[2].as<string>() 
           << ", syscall_src: " << c[3].as<string>() 
           << ", syscall_sink: " << c[4].as<string>() 
           << ", syscall_src_T: " << c[5].as<int64_t>() 
           << ", syscall_sink_T: " << c[6].as<int64_t>() 
           << ", obj_in: " << c[7].as<string>() 
           << ", obj_out: " << c[8].as<string>() 
           << ", in_uuid: " << c[9].as<u_long>()
           << ", out_uuid: " << c[10].as<u_long>()
           << "\n";                                                 
#endif
			/*we will return the merged interleavings in every process group*/
			update_procItLvGrp(proc_itlvgrp_map, pid, cmdline, 
				SyscallType(stoi(syscall_src)), syscall_src_T, obj_in, in_uuid);
			update_procItLvGrp(proc_itlvgrp_map, pid, cmdline, 
				SyscallType(stoi(syscall_sink)), syscall_sink_T, obj_out, out_uuid);
    }
		return;
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return; 
  }

}

#include <string>
#include <vector>
#include <map>
#include <set>
#include <iostream>
#include <pqxx/pqxx> 

#include "TheiaDB.h"
#include "TheiaTagging.h"

#include <cstdlib>

#define THEIA_DEBUG

using namespace std;
using namespace pqxx;

extern string psql_cred;

connection* C = NULL;
nontransaction* N = NULL;

int get_subjects_for_taint(struct subjects_for_taint **subject, string query_id)
{
  try{
    if(C != NULL){
      if (!C->is_open()) {
        delete C;
        C = new connection(psql_cred);
      }
    }
    else {
      C = new connection(psql_cred);
    }

    if (!C->is_open()) {
      cout << "Can't open database" << endl;
      return -1;
    }

    stringstream buff;
    /* Create SQL statement */
		//FIXME: the stupid postgresql does not recognize pid%cmdline in like claus...
    buff << "SELECT subject.pid, subject.path, subject.uuid from subject INNER JOIN subgraph \
            ON subject.uuid = subgraph.subject_uuid WHERE subgraph.query_id = '" 
            << query_id << "';";

#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    /* Create a non-transactional object. */
		if(N == NULL) {
			N = new nontransaction(*C);
		}

    /* Execute SQL query */
    result R( N->exec(buff.str().c_str()));

    struct subjects_for_taint* p_subjects = 
        (struct subjects_for_taint*)malloc(sizeof(struct subjects_for_taint) * R.size());
    int i = 0;
    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
			p_subjects[i].pid = c[0].as<int>(); 
			p_subjects[i].path = c[1].as<string>(); 
			p_subjects[i].subject_uuid = c[2].as<string>(); 
    }
    *subject = p_subjects;
    return R.size();
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return -1; 
  }
  
}

string get_replay_path(int pid, string cmdline) {
  try{
    if(C != NULL){
      if (!C->is_open()) {
        delete C;
        C = new connection(psql_cred);
      }
    }
    else {
      C = new connection(psql_cred);
    }

    if (!C->is_open()) {
      cout << "Can't open database" << endl;
      return "ERROR";
    }

    stringstream buff;
    /* Create SQL statement */
		//FIXME: the stupid postgresql does not recognize pid%cmdline in like claus...
    buff << "SELECT dir FROM rec_index WHERE procname LIKE '" <<  pid  << "%" << cmdline << "';";

#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    /* Create a non-transactional object. */
		if(N == NULL) {
			N = new nontransaction(*C);
		}

    /* Execute SQL query */
    result R( N->exec(buff.str().c_str()));

    if (R.size() > 1) {
      cout << "More than one replay group found, what the chance! (" << pid << cmdline << ")\n";
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

ances_uuid_vec query_ances_fuuid(u_long f_uuid, off_t offset, u_long timestamp) {
                                                                                 
  ances_uuid_vec vec;
  try{                                                                           
    if(C != NULL){
      if (!C->is_open()) {
        delete C;
        C = new connection(psql_cred);
      }
    }
    else {
      C = new connection(psql_cred);
    }

    if (!C->is_open()) {
      cout << "Can't open database" << endl;
      return vec;
    }

                                                                                 
    stringstream buff;                                                           
    /* Create SQL statement */                                                   
    buff << "SELECT * FROM file_tagging WHERE" << " f_uuid = " << f_uuid << " AND "
      << "off_t = " << offset << " AND " << "timestamp <= " << timestamp << ";";        
                                                                                 
#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    /* Create a non-transactional object. */
		if(N == NULL) {
			N = new nontransaction(*C);
		}

    /* Execute SQL query */
    result R( N->exec(buff.str().c_str()));

    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
			auto tag_uuid = c[4].as<u_long>(); 
      
      vec.push_back(tag_uuid);
    }
    
    return vec;
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return vec; 
  }
	return vec;
}

long query_file_tagging_postgres(u_long f_uuid, off_t offset, ssize_t* p_size) {
                                                                                 
  try{                                                                           
    if(C != NULL){
      if (!C->is_open()) {
        delete C;
        C = new connection(psql_cred);
      }
    }
    else {
      C = new connection(psql_cred);
    }

    if (!C->is_open()) {
      cout << "Can't open database" << endl;
      return -1;
    }

                                                                                 
    stringstream buff;                                                           
    /* Create SQL statement */                                                   
    buff << "SELECT * FROM file_tagging WHERE" << " f_uuid = " << f_uuid << " AND "
      << "off_t = " << offset << ";";        
                                                                                 
#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    /* Create a non-transactional object. */
		if(N == NULL) {
			N = new nontransaction(*C);
		}

    /* Execute SQL query */
    result R( N->exec(buff.str().c_str()));

		if(R.empty()) {
			return -1;
		}
    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
			auto entry_id = c[0].as<int>(); 
			auto size = c[3].as<ssize_t>(); 
			*p_size = size;
			return entry_id;
    }
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return -1; 
  }
	return -1;
}


int64_t get_event_timestamp(u_long f_uuid) {

  try{
    if(C != NULL){
      if (!C->is_open()) {
        delete C;
        C = new connection(psql_cred);
      }
    }
    else {
      C = new connection(psql_cred);
    }

    if (!C->is_open()) {
      cout << "Can't open database" << endl;
      return -1;
    }


    stringstream buff;
   //Create SQL statement
    buff << "SELECT * FROM SYSCALLS WHERE" << " fuuid = " << f_uuid << ";";

#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    //Create a non-transactional object.
    if(N == NULL) {
    	N = new nontransaction(*C);
    }

    //Execute SQL query
    result R(N->exec(buff.str().c_str()));

    if(R.empty()) {
    	return -1;
    }
    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
                        auto timestamp = c[3].as<int64_t>();
                        return timestamp;
    }
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return -1;
  }
  return -1;
}

void get_pid_cmdline(string sink_uuid, int *pid, string *cmdline) {

  try{
    if(C != NULL){
      if (!C->is_open()) {
        delete C;
        C = new connection(psql_cred);
      }
    }
    else {
      C = new connection(psql_cred);
    }

    if (!C->is_open()) {
      cout << "Can't open database" << endl;
      return;
    }


    stringstream buff;
   //Create SQL statement
    buff << "SELECT * FROM SYSCALLS WHERE" << " fuuid = " << sink_uuid << ";";

#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    //Create a non-transactional object.
    if(N == NULL) {
    	N = new nontransaction(*C);
    }

    //Execute SQL query
    result R(N->exec(buff.str().c_str()));

    if(R.empty()) {
    	return;
    }
    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
                        *pid = c[0].as<int>();
                        *cmdline = c[1].as<string>();
                        return;
    }
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return;
  }
  return;
}

void insert_file_tagging_postgres(u_long f_uuid, off_t offset, ssize_t size, u_long tag_uuid) {

  try{
    if(C != NULL){
      if (!C->is_open()) {
        delete C;
        C = new connection(psql_cred);
      }
    }
    else {
      C = new connection(psql_cred);
    }

    if (!C->is_open()) {
      cout << "Can't open database" << endl;
      return;
    }

    stringstream buff;

    /* Create SQL statement */
		ssize_t prev_size = 0;
		auto entry_id = query_file_tagging_postgres(f_uuid, offset, &prev_size);
		if (entry_id == -1) {
			buff << "INSERT INTO file_tagging (f_uuid, off_t, size, tag_uuid) " 
				<< "VALUES (" << f_uuid << "," << offset << "," << size << 
				"," << tag_uuid << ");";
		}
		else {
			if(prev_size == size) {
				buff << "UPDATE file_tagging SET tag_uuid = " <<
					tag_uuid << " WHERE entry_id = " << entry_id << ";";
			}
			else {
				//need to split/remerge the block.
			}
		}

#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    /* Create a transactional object. */
    work W(*C);

    /* Execute SQL query */
    W.exec( buff.str().c_str() );
    W.commit();

  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return;
  }
  
}

void insert_path_uuid_postgres(string path, uint32_t version, u_long uuid) {

  try{
    if(C != NULL){
      if (!C->is_open()) {
        delete C;
        C = new connection(psql_cred);
      }
    }
    else {
      C = new connection(psql_cred);
    }

    if (!C->is_open()) {
      cout << "Can't open database" << endl;
      return;
    }

    cout << "uuid " << uuid;
    cout << "path " << path;
    cout << "version " << version;
    stringstream buff;
    /* Create SQL statement */
    buff << "INSERT INTO path_uuid (path_name, version, uuid) " 
			<< "SELECT '" << path << "'," << version << "," << uuid << " WHERE NOT EXISTS " 
      << "(SELECT 0 FROM path_uuid where uuid = " << uuid << " and version = " << version << ");";

#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    /* Execute SQL query */
    work W(*C);
    W.exec( buff.str().c_str() );
    
    W.commit();

  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return;
  }
  
}

void insert_syscall_entry(int pid, string cmdline, SyscallStruct &syscall) {
  try{
    if(C != NULL){
      if (!C->is_open()) {
        delete C;
        C = new connection(psql_cred);
      }
    }
    else {
      C = new connection(psql_cred);
    }

    if (!C->is_open()) {
      cout << "Can't open database" << endl;
      return;
    }

    stringstream buff;
    /* Create SQL statement */
    buff << "INSERT INTO SYSCALLS (pid,cmdline,syscall,timestamp,filename,fuuid,version) " 
			<< "VALUES (" << pid << ",'" << cmdline << "'," << syscall.syscall << "," 
      << syscall.timestamp << ",'" << syscall.file_name << "'," << syscall.uuid 
      << "," << syscall.version << ");";

#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    work W(*C);
    /* Execute SQL query */
    W.exec( buff.str().c_str() );

    W.commit();

  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return;
  }
  
}

void insert_entry_postgres(int pid, string cmdline, SyscallType syscall,
    int64_t timestamp, string file_name, u_long out_uuid, SyscallStruct syscall_struct) {

  try{
    if(C != NULL){
      if (!C->is_open()) {
        delete C;
        C = new connection(psql_cred);
      }
    }
    else {
      C = new connection(psql_cred);
    }

    if (!C->is_open()) {
      cout << "Can't open database" << endl;
      return;
    }

    stringstream buff;
    /* Create SQL statement */
    buff << "INSERT INTO CLST (pid,cmdline,syscall_src,syscall_sink,\
      syscall_src_T,syscall_sink_T,obj_in,obj_out,in_uuid,out_uuid) " 
			<< "VALUES (" << pid << ",'" << cmdline << "'," << syscall_struct.syscall << "," 
      << syscall << "," << syscall_struct.timestamp << "," << timestamp 
      << ",'" << syscall_struct.file_name << "','" << file_name << "'," 
			<< syscall_struct.uuid << "," << out_uuid << ");";

#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    /* Create a transactional object. */
    work W(*C);

    /* Execute SQL query */
    W.exec( buff.str().c_str() );
    W.commit();

  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return;
  }
  
}

//void query_entry_postgres(Proc_itlv_grp_type &proc_itlvgrp_map, 
//  int64_t start_time, int64_t end_time, string obj_out) {
//                                                                                 
//  try{                                                                           
//    if(C != NULL){
//      if (!C->is_open()) {
//        delete C;
//        C = new connection(psql_cred);
//      }
//    }
//    else {
//      C = new connection(psql_cred);
//    }
//
//    if (!C->is_open()) {
//      cout << "Can't open database" << endl;
//      return;
//    }
//
//                                                                                 
//    stringstream buff;                                                           
//    /* Create SQL statement */                                                   
//    buff << "SELECT * FROM CLST WHERE" << " obj_out = '" << obj_out << "' AND "
//      << "syscall_sink_T BETWEEN " << start_time << " AND " << end_time << ";";        
//                                                                                 
//#ifdef THEIA_DEBUG
//    cout << buff.str() << "\n";
//#endif
//
//    /* Create a non-transactional object. */
//		if(N == NULL) {
//			N = new nontransaction(*C);
//		}
//
//    /* Execute SQL query */
//    result R( N->exec(buff.str().c_str()));
//
//    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
//			auto pid = c[1].as<int>(); 
//			auto cmdline = c[2].as<string>();
//			auto syscall_src = c[3].as<string>();
//			auto syscall_sink = c[4].as<string>();
//			auto syscall_src_T = c[5].as<int64_t>();
//			auto syscall_sink_T = c[6].as<int64_t>();
//			auto obj_in = c[7].as<string>();
//			auto obj_out = c[8].as<string>();
//			auto in_uuid = c[9].as<u_long>();
//			auto out_uuid = c[10].as<u_long>();
//      
//
//#ifdef THEIA_DEBUG
//      cout << "pid: " << c[1].as<int>() 
//           << ", cmdline: " << c[2].as<string>() 
//           << ", syscall_src: " << c[3].as<string>() 
//           << ", syscall_sink: " << c[4].as<string>() 
//           << ", syscall_src_T: " << c[5].as<int64_t>() 
//           << ", syscall_sink_T: " << c[6].as<int64_t>() 
//           << ", obj_in: " << c[7].as<string>() 
//           << ", obj_out: " << c[8].as<string>() 
//           << ", in_uuid: " << c[9].as<u_long>()
//           << ", out_uuid: " << c[10].as<u_long>()
//           << "\n";                                                 
//#endif
//			/*we will return the merged interleavings in every process group*/
//			update_procItLvGrp(proc_itlvgrp_map, pid, cmdline, 
//				SyscallType(stoi(syscall_src)), syscall_src_T, obj_in, in_uuid, 0);
//			update_procItLvGrp(proc_itlvgrp_map, pid, cmdline, 
//				SyscallType(stoi(syscall_sink)), syscall_sink_T, obj_out, out_uuid, 0);
//    }
//		return;
//  } catch (const std::exception &e){
//    cerr << e.what() << std::endl;
//    return; 
//  }
//
//}
u_long query_uuid_postgres(string path, uint32_t version) {
                                                                                 
  try{                                                                           
    if(C != NULL){
      if (!C->is_open()) {
        delete C;
        C = new connection(psql_cred);
      }
    }
    else {
      C = new connection(psql_cred);
    }

    if (!C->is_open()) {
      cout << "Can't open database" << endl;
      return 0;
    }

                                                                                 
    stringstream buff;                                                           
    /* Create SQL statement */                                                   
    buff << "SELECT uuid FROM path_uuid WHERE" << " path_name LIKE '%" << path 
			<< "' AND version = " << version << ";";        
                                                                                 
#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    /* Create a non-transactional object. */
		if(N == NULL) {
			N = new nontransaction(*C);
		}

    /* Execute SQL query */
    result R( N->exec(buff.str().c_str()));

    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
			auto uuid = c[0].as<u_long>();
      

#ifdef THEIA_DEBUG
      cout << "uuid: " << c[0].as<u_long>() << "\n";                                                 
#endif
			return uuid;
    }
		return 0;
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return 0; 
  }

}
std::set<u_long> query_uuid_set_postgres(string path, uint32_t version) {
  std::set<u_long> uuid_results;                                                                                 
  try{                                                                           
    if(C != NULL){
      if (!C->is_open()) {
        delete C;
        C = new connection(psql_cred);
      }
    }
    else {
      C = new connection(psql_cred);
    }

    if (!C->is_open()) {
      cout << "Can't open database" << endl;
      return uuid_results;
    }
                                                                                 
    stringstream buff;                                                           
    /* Create SQL statement */                                                   
    buff << "SELECT uuid FROM path_uuid WHERE" << " path_name = '" << path 
			<< "' AND version = " << version << ";";        
                                                                                 
#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    /* Create a non-transactional object. */
		if(N == NULL) {
			N = new nontransaction(*C);
		}

    /* Execute SQL query */
    result R( N->exec(buff.str().c_str()));

    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
			auto uuid = c[0].as<u_long>();
	uuid_results.insert(uuid);
      

#ifdef THEIA_DEBUG
      cout << "uuid: " << c[0].as<u_long>() << "\n";                                                 
#endif
    }
		return uuid_results;
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return uuid_results; 
  }

}

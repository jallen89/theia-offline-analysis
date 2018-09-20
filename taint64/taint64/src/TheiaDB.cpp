#include <string>
#include <vector>
#include <map>
#include <set>
#include <iostream>
#include <pqxx/pqxx> 
#include <string.h>

#include "TheiaDB.h"
#include "TheiaTagging.h"

#include <cstdlib>

#define THEIA_DEBUG

using namespace std;
using namespace pqxx;

string psql_cred = "dbname=theia.db \
                    user=theia \
                    password=darpatheia1 \
                    host=127.0.0.1 \
                    port=5432";

connection* C = NULL;
nontransaction* N = NULL;

CDM_UUID_Type get_cdm_uuid(string uuid_str)                                      
{                                                                                
  string substring = uuid_str;                                                   
  CDM_UUID_Type uuid;                                                            
  int index = 0;                                                                 
  while(true) {                                                                  
    string unit = (substring.substr(0, substring.find(' ')));                    
    uuid[index] = stoi(unit);                                                    
    index++;                                                                     
    size_t pos = substring.find(' ');                                               
    if(pos == string::npos)                                                      
      break;                                                                     
    substring = substring.substr(pos+1);                                         
  }                                                                              
  return uuid;                                                                   
}

int get_global_tags_from_rdb(string query_id, string subject_uuid, 
  string local_tag, string &glb_tag)
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
    buff << "SELECT global_tag FROM tag_mapping WHERE" << " query_id = '" 
      << query_id << "' AND local_tag = '" << local_tag 
      << "' AND subject_uuid = '" << subject_uuid << "';";        
                                                                                 
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
			return 0;
		}
    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
      glb_tag = c[0].as<string>();
			return R.size();
    }
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return -1; 
  }
	return -1;
}

int store_local_tags_to_rdb(string query_id, string subject_uuid, 
  string local_tag, string global_tag)
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
    string prev_glb_tag;
		int ret_size = get_global_tags_from_rdb(query_id, 
      subject_uuid, local_tag, prev_glb_tag);
		if (ret_size <= 0) {
			buff << "INSERT INTO tag_mapping (query_id, subject_uuid, global_tag, local_tag) " 
				<< "VALUES ('" << query_id << "','" << subject_uuid << "','" << global_tag << 
				"','" << global_tag << "');";
		}
		else {
      buff << "UPDATE tag_mapping SET global_tag = '"
        << global_tag << " WHERE query_id = '" << query_id 
        << "' AND subject_uuid = '" << subject_uuid << "' AND local_tag = '" 
        << local_tag << "';";
		}

#ifdef THEIA_DEBUG
    cout << buff.str() << "\n";
#endif

    /* Create a transactional object. */
    work W(*C);

    /* Execute SQL query */
    W.exec( buff.str().c_str() );
    W.commit();
    return 0;

  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return -1;
  }
  
}

int get_inbound_for_taint(string query_id, string subject_uuid, CDM_UUID_Type **inb_uuid)
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
    buff << "SELECT file_uuid, netflow_uuid from subgraph \
            WHERE query_id = '" << query_id << "' AND subject_uuid = '" 
            << subject_uuid << "' AND (event_type = 'EVENT_READ' OR event_type = \
            'EVENT_RECV');";

#ifdef THEIA_DEBUG
//    cout << buff.str() << "\n";
#endif

    /* Create a non-transactional object. */
		if(N == NULL) {
			N = new nontransaction(*C);
		}

    /* Execute SQL query */
    result R( N->exec(buff.str().c_str()));

    CDM_UUID_Type* p_uuids = 
        (CDM_UUID_Type*)malloc(sizeof(CDM_UUID_Type) * R.size());
    int i = 0;
    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
      string file_uuid = c[0].as<string>();
      string netflow_uuid = c[1].as<string>();
			if(file_uuid.size() == 0) {
        p_uuids[i] = get_cdm_uuid(netflow_uuid);
      } 
      else {
        p_uuids[i] = get_cdm_uuid(file_uuid);
      }
      i++;
    }
    *inb_uuid = p_uuids;
    return R.size();
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return -1; 
  }
}

int get_outbound_for_taint(string query_id, string subject_uuid, CDM_UUID_Type **out_uuid)
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
    buff << "SELECT file_uuid, netflow_uuid from subgraph \
            WHERE query_id = '" << query_id << "' AND subject_uuid = '" 
            << subject_uuid << "' AND (event_type = 'EVENT_WRITE' OR event_type = \
            'EVENT_SEND');";

#ifdef THEIA_DEBUG
//    cout << buff.str() << "\n";
#endif

    /* Create a non-transactional object. */
		if(N == NULL) {
			N = new nontransaction(*C);
		}

    /* Execute SQL query */
    result R( N->exec(buff.str().c_str()));

    CDM_UUID_Type* p_uuids = 
        (CDM_UUID_Type*)malloc(sizeof(CDM_UUID_Type) * R.size());
    int i = 0;
    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
      string file_uuid = c[0].as<string>();
      string netflow_uuid = c[1].as<string>();
			if(file_uuid.size() == 0) {
        p_uuids[i] = get_cdm_uuid(netflow_uuid);
      } 
      else {
        p_uuids[i] = get_cdm_uuid(file_uuid);
      }
      i++;
    }
    *out_uuid = p_uuids;
    return R.size();
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return -1; 
  }
}

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
    buff << "SELECT DISTINCT subject.pid, subject.path, subject.uuid, subject.local_principal from subject INNER JOIN subgraph \
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

    int subjects_num = 0;
    subjects_num = R.size();

#ifdef THEIA_DEBUG                                                               
     cout << "subjects num=" << subjects_num << "\n";                                                       
#endif 

    //early return
    if(subjects_num==0){
      return subjects_num;
    }

    struct subjects_for_taint* p_subjects = (struct subjects_for_taint*) malloc(sizeof(struct subjects_for_taint)*subjects_num);
    memset(p_subjects, 0, sizeof(struct subjects_for_taint)*subjects_num);
    int i = 0;
    for (result::const_iterator c = R.begin(); c != R.end(); ++c) {
     int pid = c[0].as<int>();
     string path = c[1].as<string>();
     string subject_uuid = c[2].as<string>();
     string local_principal = c[3].as<string>();                   
#ifdef THEIA_DEBUG                                             
     cout << pid << "#" << path << "#" << subject_uuid << "#" << local_principal << "\n";
#endif  
    	p_subjects[i].pid = pid; 
			strncpy(p_subjects[i].path, path.c_str(), path.length()); 
			strncpy(p_subjects[i].subject_uuid, subject_uuid.c_str(), subject_uuid.length());
      strncpy(p_subjects[i].local_principal, local_principal.c_str(), local_principal.length());
      i++;
    }

    *subject = p_subjects;

    return subjects_num;
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

bool is_netflowuuid_same(char *local_ip_1, 
                         uint16_t local_port_1,
                         char *remote_ip_1,
                         uint16_t remote_port_1,
                         char* local_ip_2,
                         uint16_t local_port_2,
                         char* remote_ip_2,
                         uint16_t remote_port_2) {

  if(strcmp(local_ip_1, "NA") == 0 ||
     strcmp(local_ip_2, "NA") == 0 ||
     strcmp(remote_ip_1, "NA") == 0 ||
     strcmp(remote_ip_2, "NA") == 0 ||
     strcmp(local_ip_1, "LOCAL") == 0 ||
     strcmp(local_ip_2, "LOCAL") == 0 ||
     strcmp(remote_ip_1, "LOCAL") == 0 ||
     strcmp(remote_ip_2, "LOCAL") == 0 
    ) {
      return false;
  }
  if(strcmp(local_ip_1,remote_ip_2) == 0 && 
     strcmp(local_ip_2, remote_ip_1) == 0 &&
     local_port_1 == remote_port_2 &&
     local_port_2 == remote_port_1) {
      return true;
  }

  return false;
}

void parse_uuids(string result_uuids, set<string>& result_tag_uuid_set)
{
  string sub_str = result_uuids;
  size_t i;
  while((i = sub_str.find("|"))!=string::npos) {
    result_tag_uuid_set.insert(sub_str.substr(0,i-1));
    sub_str = sub_str.substr(i+1);
  }
  result_tag_uuid_set.insert(sub_str);
}

void theia_tag_overlay_query(string uuid, 
                              uint32_t offset, 
                              set<string>& result_tag_uuid_set,
                              string qid) 
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
      return; 
    }
                                                                                 
    stringstream buff;                                                           
    /* Create SQL statement */                                                   
    buff << "SELECT origin_uuids FROM tag_overlay WHERE" << " uuid = '" << uuid 
			<< "' AND offset = " << offset << " AND qid = '" << qid << "';";        
                                                                                 
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
			auto result_uuids = c[0].as<string>();

    parse_uuids(result_uuids, result_tag_uuid_set); 
      

#ifdef THEIA_DEBUG
      cout << "uuid: " << c[0].as<u_long>() << "\n";                                                 
#endif
    }
		return;
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return; 
  }

}

string pack_uuids(set<string> result_tag_uuid_set)
{
  string result = "";
  for(auto it=result_tag_uuid_set.begin();it!=result_tag_uuid_set.end();it++){
    result.append(*it);
    result.append("|");
  } 
  result.erase(result.end()-1);
  return result;
}

void theia_tag_overlay_insert(string uuid, 
                              uint32_t offset, 
                              string type,
                              set<string> origin_uuids,
                              string qid,
                              string tag_uuid_str,
                              string subject_uuid_str) 
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
      return;
    }

    stringstream buff;

    /* Create SQL statement */
		set<string> prev_origin_uuids;
		theia_tag_overlay_query(uuid, offset, prev_origin_uuids, qid);
    string str_origin_uuids = pack_uuids(origin_uuids);
		if (!prev_origin_uuids.empty()) {
			buff << "INSERT INTO tag_overlay (uuid, type, offset, origin_uuids, qid, tag_uuid, subject_uuid) " 
				<< "VALUES ('" << uuid << "','" << type << "'," << offset << ",'" << str_origin_uuids << "', '" << qid << "', '" << tag_uuid_str << "', '" << subject_uuid_str << "');";
		}
		else {
      buff << "UPDATE tag_overlay SET origin_uuids = '"
        << str_origin_uuids << "' WHERE uuid = '" << uuid << "' AND type = '" << type 
        << "' AND offset = " << offset << " AND qid = '" << qid << "';";
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

extern string uuid_to_string(CDM_UUID_Type uuid);

extern CDM_UUID_Type get_netflow_uuid(string lip, uint16_t lport, 
    string rip, uint16_t rport);

string search_cross_tag(string rip, uint16_t rport, string lip, uint16_t lport, set<uint32_t>& tags)
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
      return ""; 
    }
    
    CDM_UUID_Type uuid1;
    stringstream buff1, buff2;
    /* Create SQL statement */
    buff1 << "SELECT tag FROM cross-tags WHERE" << "lip = '" << rip 
			<< "' AND rip = '" << lip << "' AND lport = " << rport << "AND rport = "
      << lport << ";";

#ifdef THEIA_DEBUG
    cout << buff1.str() << "\n";
#endif

    /* Create a non-transactional object. */
		if(N == NULL) {
			N = new nontransaction(*C);
		}

    /* Execute SQL query */
    result R1( N->exec(buff1.str().c_str()));

    if(!R1.empty()) {
      uuid1 = get_netflow_uuid(rip, rport, lip, lport);
    }
    set<uint32_t> tags1;
    for (result::const_iterator c = R1.begin(); c != R1.end(); ++c) {
			tags1.insert(c[0].as<uint32_t>());

#ifdef THEIA_DEBUG
      cout << "uuid: " << uuid_to_string(uuid1) << "\n";                                                 
#endif
    }

    buff2 << "SELECT tag FROM cross-tags WHERE" << "lip = '" << lip 
			<< "' AND rip = '" << rip << "' AND lport = " << lport << "AND rport = "
      << rport << ";";

    result R2( N->exec(buff1.str().c_str()));

    set<uint32_t> tags2;
    for (result::const_iterator c = R2.begin(); c != R2.end(); ++c) {
			tags2.insert(c[0].as<uint32_t>());
    }

    for(auto it1=tags1.begin();it1!=tags1.end();it1++){
      for(auto it2=tags2.begin();it2!=tags2.end();it2++){
        if(*it1 == *it2) {
          tags.insert(*it1);
        }
      }
    }

		return uuid_to_string(uuid1); 
  } catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return ""; 
  }

}




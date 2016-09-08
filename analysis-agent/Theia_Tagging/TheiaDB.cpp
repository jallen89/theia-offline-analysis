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
      << timestamp << "," << syscall_struct.file_name << ",'" << file_name << "');";

    /* Create a transactional object. */
    work W(C);

    /* Execute SQL query */
    W.exec( buff.str().c_str() );
    W.commit();

  }catch (const std::exception &e){
    cerr << e.what() << std::endl;
    return;
  }


}

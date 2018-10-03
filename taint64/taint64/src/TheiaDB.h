#ifndef __THEIADB_H__
#define __THEIADB_H__

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <pqxx/pqxx> 
#include <set>
#include <boost/array.hpp>

#include "TheiaTagging.h"

using namespace std;
using namespace pqxx;

typedef vector<u_long> ances_uuid_vec;

typedef boost::array<uint8_t, 16> CDM_UUID_Type;

struct subjects_for_taint
{
  int pid;
  char path[4096];
  char subject_uuid[64];
  char local_principal[64];
};

string get_replay_path(int pid, string cmdline);

void insert_path_uuid_postgres(string path, uint32_t version, u_long uuid);
u_long query_uuid_postgres(string path, uint32_t version);
set<u_long> query_uuid_set_postgres(string path, uint32_t version);

void insert_entry_postgres(int pid, string cmdline, SyscallType syscall,
    int64_t timestamp, string file_name, u_long uuid, SyscallStruct syscall_struct);

void query_entry_postgres(Proc_itlv_grp_type &proc_itlvgrp_map, 
  int64_t start_time, int64_t end_time, string obj_out);

long query_file_tagging_postgres(u_long f_uuid, off_t offset, ssize_t* p_size);
void insert_file_tagging_postgres(u_long f_uuid, off_t offset, ssize_t size, u_long tag_uuid);
int64_t get_event_timestamp(u_long f_uuid);

void get_pid_cmdline(string sink_uuid, int *pid, string *cmdline);

ances_uuid_vec query_ances_fuuid(u_long f_uuid, off_t offset, u_long timestamp);
//extern Theia_db_postgres theia_db;

void insert_syscall_entry(int pid, string cmdline, SyscallStruct &syscall);


/*for 64bit taint tags */
CDM_UUID_Type get_cdm_uuid(string uuid_str);
int get_inbound_for_taint(string query_id, string subject_uuid, CDM_UUID_Type **inb_uuid);
int get_outbound_for_taint(string query_id, string subject_uuid, CDM_UUID_Type **out_uuid);
int get_subjects_for_taint(struct subjects_for_taint **subject, string query_id);

int get_global_tags_from_rdb(string query_id, string subject_uuid, 
  string local_tag, string &glb_tag);
int store_local_tags_to_rdb(string query_id, string subject_uuid, 
  string local_tag, string global_tag);

//tag overlay
void theia_tag_overlay_insert(string uuid, 
                              string type,
                              set<string> origin_uuids,
                              string qid,
                              string tag_uuid_str,
                              string subject_uuid_str) ;

void theia_tag_overlay_query(string uuid, 
                              set<string>& result_tag_uuid_set,
                              string qid);

string search_cross_tag(string rip, uint16_t rport, string lip, uint16_t lport, set<uint32_t>& tags);
#endif

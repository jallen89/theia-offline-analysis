/*-
 * Copyright (c) 2010, 2011, 2012, 2013, Columbia University
 * All rights reserved.
 *
 * This software was developed by Vasileios P. Kemerlis <vpk@cs.columbia.edu>
 * at Columbia University, New York, NY, USA, in October 2010.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Columbia University nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * TODO:
 * 	- add support for file descriptor duplication via fcntl(2)
 * 	- add support for non PF_INET* sockets
 * 	- add support for recvmmsg(2)
 */

#include <errno.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <iterator>
#include <vector>

#include <set>
#include <assert.h>
#include <map>

#include "TheiaDB.h"
#include "branch_pred.h"
#include "libdft_api64.h"
#include "libdft_core64.h"
#include "syscall_desc64.h"
#include "tagmap64.h"
#include "dta_api64.h"
#include "debuglog.h"

#define QUAD_LEN    8
#define LONG_LEN	4	/* size in bytes of a word value */

/* default suffixes for dynamic shared libraries */
#define DLIB_SUFF	".so"
#define DLIB_SUFF_ALT	".so."

/* set of interesting descriptors (sockets) */
static set<int> fdset;

//mf: commented them out as they are not used so far
//static AFUNPTR alert_reg = NULL;
//static AFUNPTR alert_mem = NULL;
//static FILENAME_PREDICATE_CALLBACK filename_predicate = NULL;
//static NETWORK_PREDICATE_CALLBACK network_predicate = NULL;

//mf: commented
//#define DEBUG_PRINT_TRACE
#ifdef DEBUG_PRINT_TRACE
#include "debuglog.h"
#endif


//counter to label each byte read differently
unit_tag_t tag_counter = 0;
map<string, unit_tag_t> uuid_to_tag;
map<unit_tag_t, string> tag_to_tag_uuid;
set<string> generated_tag_uuid_set;

CDM_UUID_Type get_uuid_array_from_value(unsigned long long value){
    boost::array<uint8_t, 16> uuid_array = {};
    size_t i=0;                              
    for(i=0; i<sizeof(value); ++i) {                                
      uuid_array[i] = ((value >> (i*8)) & 0xff);                        
    }
    return uuid_array;
}

CDM_UUID_Type string_to_uuid(const string uuid_string)
{
  std::istringstream buf(uuid_string);
  std::istream_iterator<std::string> beg(buf), end;
  std::vector<std::string> tokens(beg, end);
  boost::array<uint8_t, 16> uuid_array = {};
  int index = 0;
  for(auto s:tokens){
  	if(index>15){
		break;	
	}
  	unsigned long ul_value = strtoul(s.c_str(), NULL, 0);
        uint8_t uint8_value = (uint8_t) ul_value;
        uuid_array[index] = uint8_value;
        index++;
	
  }
  return uuid_array;
}

int publish_data_to_kafka(std::string key, tc_schema::TCCDMDatum record){
	if(publish_to_kafka_global){
		producer_global->publish_record(key, record);
	}

	if(create_avro_file_global){
		file_serializer_global->serializeToFile(record);
	}

	return 0;
}

tc_schema::ProvenanceTagNode create_cdm_provenance_tag_node(string tag_uuid, string object_uuid, set<string> source_tag_uuid_set, string system_call){
	tc_schema::ProvenanceTagNode new_provenance_tag_node;

	CDM_UUID_Type tag_uuid_array = string_to_uuid(tag_uuid);
	new_provenance_tag_node.tagId = tag_uuid_array;

	CDM_UUID_Type object_uuid_array = string_to_uuid(object_uuid);
	new_provenance_tag_node.flowObject.set_UUID(object_uuid_array);

	CDM_UUID_Type host_uuid_array = string_to_uuid("0");
	new_provenance_tag_node.hostId = host_uuid_array;

	CDM_UUID_Type subject_uuid_array = string_to_uuid(subject_uuid_global);
	new_provenance_tag_node.subject = subject_uuid_array;

	new_provenance_tag_node.systemCall.set_string(system_call);

	new_provenance_tag_node.opcode.set_TagOpCode(tc_schema::TagOpCode::TAG_OP_UNION);

	std::vector<boost::array<uint8_t, 16> > source_tag_array_vector;
	for(auto source_tag_uuid:source_tag_uuid_set){
		CDM_UUID_Type source_tag_uuid_array = string_to_uuid(source_tag_uuid);
		source_tag_array_vector.push_back(source_tag_uuid_array);
	}
	new_provenance_tag_node.tagIds.set_array(source_tag_array_vector);

	return new_provenance_tag_node;
}

void theia_store_cdm_provenance_tag_node(string tag_uuid, string object_uuid, set<string> source_tag_uuid_set, string system_call){
	tc_schema::ProvenanceTagNode new_provenance_tag_node = create_cdm_provenance_tag_node(tag_uuid, object_uuid, source_tag_uuid_set, system_call);
	tc_schema::TCCDMDatum new_provenance_tag_node_record;
	new_provenance_tag_node_record.datum.set_ProvenanceTagNode(new_provenance_tag_node);
	new_provenance_tag_node_record.CDMVersion = CURR_CDM_VERSION;
	new_provenance_tag_node_record.source = tc_schema::InstrumentationSource::SOURCE_LINUX_THEIA;
	publish_data_to_kafka(default_key_global, new_provenance_tag_node_record);
}

void theia_store_cdm_query_result(){
	//write end of query to kafka
        tc_schema::TheiaQueryResult new_theia_query_result;

	CDM_UUID_Type query_id_array = string_to_uuid(query_id_global);
	new_theia_query_result.queryId = query_id_array;

	std::vector<boost::array<uint8_t, 16> > generated_tag_array_vector;
	for(auto generated_tag_uuid:generated_tag_uuid_set){
		CDM_UUID_Type generated_tag_uuid_array = string_to_uuid(generated_tag_uuid);
		generated_tag_array_vector.push_back(generated_tag_uuid_array);
	}
	new_theia_query_result.tagIds = generated_tag_array_vector;

	tc_schema::TCCDMDatum new_theia_query_result_record;
	new_theia_query_result_record.datum.set_TheiaQueryResult(new_theia_query_result);
	new_theia_query_result_record.CDMVersion = CURR_CDM_VERSION;
	new_theia_query_result_record.source = tc_schema::InstrumentationSource::SOURCE_LINUX_THEIA;
	publish_data_to_kafka(default_key_global, new_theia_query_result_record);
}

/*
 * read(2) handler (taint-source)
 */
void
post_read_hook(syscall_ctx_t *ctx)
{
    /* read() was not successful; optimized branch by not doing taint*/
    if (unlikely((long)ctx->ret <= 0))
    	return;

    if(engagement_config){
#ifdef THEIA_REPLAY_COMPENSATION
      logprintf("[read syscall] fd is %lu\n", ctx->arg[SYSCALL_ARG0]);
      CDM_UUID_Type uuid = get_current_uuid(NULL,NULL,NULL,NULL);
      string read_file_uuid_string = uuid_to_string(uuid); 
      logprintf("[read syscall] file UUID is %s\n", read_file_uuid_string.c_str());
      int inbound_index = -1;
      for(int i=0; i<inbound_uuid_array_count_global; ++i){
        if(uuid_to_string(inbound_uuid_array_global[i])==read_file_uuid_string){
          inbound_index = i;
          break;
        }
      }
      if(inbound_index>-1){
        logprintf("[read syscall] tainting this read\n");
        unit_tag_t tag_for_file = 0;
        if(uuid_to_tag.find(read_file_uuid_string)!=uuid_to_tag.end()){
          tag_for_file = uuid_to_tag[read_file_uuid_string];
        }
        else{
          tag_for_file = tag_counter;
          uuid_to_tag[read_file_uuid_string]=tag_for_file;
          tag_counter++;
          unsigned long long tag_uuid_value = tag_counter_global;
          CDM_UUID_Type tag_uuid = get_uuid_array_from_value(tag_uuid_value);
          string tag_uuid_string = uuid_to_string(tag_uuid);
          tag_to_tag_uuid[tag_for_file] = tag_uuid_string;
          generated_tag_uuid_set.insert(tag_uuid_string);
          tag_counter_global++;
          logprintf("[read syscall] created new tag %lu with tag UUID %s\n", tag_for_file, tag_uuid_string.c_str());
          //send provenance node to kafka
          set<string> source_tag_uuid_set;
          theia_store_cdm_provenance_tag_node(tag_uuid_string, read_file_uuid_string, source_tag_uuid_set, "EVENT_READ");
        }
        size_t addr = ctx->arg[SYSCALL_ARG1];                                      
        size_t num = ctx->ret;                                                     
        for (size_t i = addr; i < addr + num; i++){                        
          logprintf("[read syscall] set address %lx with tag %lu\n", i, tag_for_file);
          tag_t tags = {tag_for_file};                                              
          tagmap_setb_with_tags(i, tags);                                                                                               
        }
      }
      else{
        logprintf("[read syscall] not tainting this read\n");
      }
#endif
  }
  else{
    logprintf("[read syscall] not in engagement config\n");
    logprintf("[read syscall] fd is %lu\n", ctx->arg[SYSCALL_ARG0]);
    logprintf("[read syscall] tainting this read\n");
    unit_tag_t tag_for_file = 0;
    tag_for_file = tag_counter_global;
    size_t addr = ctx->arg[SYSCALL_ARG1];                                      
    size_t num = ctx->ret;                                                     
    for (size_t i = addr; i < addr + num; i++){                        
      logprintf("[read syscall] set address %lx with tag %lu\n", i, tag_for_file);
      tag_t tags = {(unit_tag_t)tag_for_file};                                              
      tagmap_setb_with_tags(i, tags);                                                                                               
    }
    tag_counter_global++;
  }
}

/*
 * recvfrom(2) handler (taint-source)
 */
void
post_recvfrom_hook(syscall_ctx_t *ctx)
{
    /* recvfrom() was not successful; optimized branch by not doing taint*/
    if (unlikely((long)ctx->ret <= 0))
        return;
    if(engagement_config){
#ifdef THEIA_REPLAY_COMPENSATION
    logprintf("[recvfrom syscall] fd is %lu\n", ctx->arg[SYSCALL_ARG0]);
    string rip = "", lip = "";
    uint16_t rport = 0, lport = 0;
    string remote_netflow_uuid;
    set<uint32_t> tags;
    CDM_UUID_Type uuid = get_current_uuid(&rip, &rport, &lip, &lport);
    //if the rip,rport,lip,lport are in pair with another one in cross-tag db, it is a cross-host match.
    if(rip.size()!=0 && lip.size()!=0 && rport != 0 && lport != 0) {
      remote_netflow_uuid = search_cross_tag(rip,rport,lip,lport,tags);
    }
    string recvfrom_network_uuid_string = uuid_to_string(uuid);
    logprintf("[recvfrom syscall] network UUID is %s\n", recvfrom_network_uuid_string.c_str());
    int inbound_index = -1;
    for(int i=0; i<inbound_uuid_array_count_global; ++i){
      if(uuid_to_string(inbound_uuid_array_global[i])==recvfrom_network_uuid_string){
        inbound_index = i;
        break;
      }
    }
    if(inbound_index>-1){
      logprintf("[recvfrom syscall] tainting this recvfrom\n");
      unit_tag_t tag_for_network = 0;
      if(uuid_to_tag.find(recvfrom_network_uuid_string)!=uuid_to_tag.end()){
        tag_for_network = uuid_to_tag[recvfrom_network_uuid_string];
      }
      else{
        tag_for_network = tag_counter;
        uuid_to_tag[recvfrom_network_uuid_string]=tag_for_network;
        tag_counter++;
        unsigned long long tag_uuid_value = tag_counter_global;
        CDM_UUID_Type tag_uuid = get_uuid_array_from_value(tag_uuid_value);
        string tag_uuid_string = uuid_to_string(tag_uuid);
        tag_to_tag_uuid[tag_for_network] = tag_uuid_string;
        generated_tag_uuid_set.insert(tag_uuid_string);
        tag_counter_global++;
        logprintf("[recvfrom syscall] created new tag %lu with tag UUID %s\n", tag_for_network, tag_uuid_string.c_str());
        //send provenance node to kafka
        set<string> source_tag_uuid_set;
        theia_store_cdm_provenance_tag_node(tag_uuid_string, recvfrom_network_uuid_string, source_tag_uuid_set, "EVENT_RECVFROM");
      }
      size_t addr = ctx->arg[SYSCALL_ARG1];
      size_t num = ctx->ret;
      for (size_t i = addr; i < addr + num; i++){
        logprintf("[recvfrom syscall] set address %lx with tag %lu\n", i, tag_for_network);
        tag_t tags = {tag_for_network};
        tagmap_setb_with_tags(i, tags);
      }
    }
    else{
      logprintf("[recvfrom syscall] not tainting this recvfrom\n");
    }
#endif
  }
  else{
    logprintf("[recvfrom syscall] not in engagement config\n");
  }
}

/*
 * write(2) handler (taint-sink)
 */
void
post_write_hook(syscall_ctx_t *ctx)
{
    /* write() was not successful; optimized branch by not doing taint*/
    if (unlikely((long)ctx->ret <= 0))
    	return;
    if(engagement_config){
#ifdef THEIA_REPLAY_COMPENSATION
    logprintf("[write syscall] fd is %lu\n", ctx->arg[SYSCALL_ARG0]);
    CDM_UUID_Type uuid = get_current_uuid(NULL,NULL,NULL,NULL);
    string write_file_uuid_string = uuid_to_string(uuid);
    logprintf("[write syscall] file UUID is %s\n", write_file_uuid_string.c_str());
    int outbound_index = -1;
    for(int i=0; i<outbound_uuid_array_count_global; ++i){
      if(uuid_to_string(outbound_uuid_array_global[i])==write_file_uuid_string){
        outbound_index = i;
        break;
      }
    }
    if(outbound_index>-1){
      logprintf("[write syscall] computing taint for write\n");
      size_t addr = ctx->arg[SYSCALL_ARG1];
      size_t num = ctx->ret;
      set<string> result_tag_uuid_set;
      for (size_t i = addr; i < addr + num; i++){
        tag_t tags = tagmap_getb(i);
        size_t tags_count = 0;
        for (tag_t::iterator it=tags.begin(); it!=tags.end(); it++){
          uint32_t tag = *it;
          logprintf("[write syscall] got tag %lu for address %lx\n", tag, i);
          tags_count++;
          string tag_uuid_string = tag_to_tag_uuid[tag];
          result_tag_uuid_set.insert(tag_uuid_string);
        }
        //update tag overlay
        theia_tag_overlay_insert(write_file_uuid_string, i, "WRITE", result_tag_uuid_set);

        if(tags_count==0){
          logprintf("[write syscall] got tag no_tag for address %lx\n", i);
        }
      }

      //alwasy generate a new tag uuid and do not save it in the map
      unsigned long long tag_uuid_value = tag_counter_global;
      CDM_UUID_Type tag_uuid = get_uuid_array_from_value(tag_uuid_value);
      string tag_uuid_string = uuid_to_string(tag_uuid);
      generated_tag_uuid_set.insert(tag_uuid_string);
      tag_counter_global++;
      logprintf("[write syscall] created new tag UUID %s\n", tag_uuid_string.c_str());
      //send tag provenance node
      //theia_store_cdm_provenance_tag_node(tag_uuid_string, write_file_uuid_string, result_tag_uuid_set, "EVENT_WRITE");
    }
    else{
      logprintf("[write syscall] not computing taint for write\n");
    }
#endif
  }
  else{
    logprintf("[write syscall] not in engagement config\n");
    logprintf("[write syscall] computing taint for write\n");
    size_t addr = ctx->arg[SYSCALL_ARG1];
    size_t num = ctx->ret;
    set<string> result_tag_uuid_set;
    for (size_t i = addr; i < addr + num; i++){
      tag_t tags = tagmap_getb(i);
      size_t tags_count = 0;
      for (tag_t::iterator it=tags.begin(); it!=tags.end(); it++){
        uint32_t tag = *it;
        logprintf("[write syscall] got tag %lu for address %lx\n", tag, i);
        tags_count++;
      }
      if(tags_count==0){
        logprintf("[write syscall] got tag no_tag for address %lx\n", i);
      }
    }
  }
}

/*
 * sendto(2) handler (taint-sink)
 */
void
post_sendto_hook(syscall_ctx_t *ctx)
{   
    /* write() was not successful; optimized branch by not doing taint*/
    if (unlikely((long)ctx->ret <= 0))
        return;
    if(engagement_config){
#ifdef THEIA_REPLAY_COMPENSATION
    logprintf("[sendto syscall] fd is %lu\n", ctx->arg[SYSCALL_ARG0]);
    CDM_UUID_Type uuid = get_current_uuid(NULL,NULL,NULL,NULL);
    string sendto_network_uuid_string = uuid_to_string(uuid);
    logprintf("[sendto syscall] network UUID is %s\n", sendto_network_uuid_string.c_str());
    int outbound_index = -1;
    for(int i=0; i<outbound_uuid_array_count_global; ++i){
      if(uuid_to_string(outbound_uuid_array_global[i])==sendto_network_uuid_string){
        outbound_index = i;
        break;
      }
    }
    if(outbound_index>-1){
      logprintf("[sendto syscall] computing taint for sendto\n");
      size_t addr = ctx->arg[SYSCALL_ARG1];
      size_t num = ctx->ret;
      set<string> result_tag_uuid_set; 
      for (size_t i = addr; i < addr + num; i++){
        tag_t tags = tagmap_getb(i);
        size_t tags_count = 0;
        for (tag_t::iterator it=tags.begin(); it!=tags.end(); it++){
          uint32_t tag = *it;
          logprintf("[sendto syscall] got tag %lu for address %lx\n", tag, i);
          tags_count++;
          string tag_uuid_string = tag_to_tag_uuid[tag];
          result_tag_uuid_set.insert(tag_uuid_string);
        }
        //update tag overlay
        theia_tag_overlay_insert(sendto_network_uuid_string, i, "SEND", result_tag_uuid_set);

        if(tags_count==0){
          logprintf("[sendto syscall] got tag no_tag for address %lx\n", i);
        }
      }
      
      //alwasy generate a new tag uuid and do not save it in the map
      unsigned long long tag_uuid_value = tag_counter_global;
      CDM_UUID_Type tag_uuid = get_uuid_array_from_value(tag_uuid_value);
      string tag_uuid_string = uuid_to_string(tag_uuid);
      generated_tag_uuid_set.insert(tag_uuid_string);
      tag_counter_global++;
      logprintf("[sendto syscall] created new tag UUID %s\n", tag_uuid_string.c_str());

      //send tag provenance node
      //theia_store_cdm_provenance_tag_node(tag_uuid_string, sendto_network_uuid_string, result_tag_uuid_set, "EVENT_SENDTO");
    }
    else{
      logprintf("[sendto syscall] not computing taint for sendto\n");
    }
#endif
  }
  else{
    logprintf("[sendto syscall] not in engagement config\n");
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////

/*
 * auxiliary (helper) function
 *
 * whenever open(2)/creat(2) is invoked,
 * add the descriptor inside the monitored
 * set of descriptors
 *
 * NOTE: it does not track dynamic shared
 * libraries
 */
void
post_open_hook(syscall_ctx_t *ctx)
{
// #ifndef USE_CUSTOM_TAG
// 	/* not successful; optimized branch */
// 	if (unlikely((long)ctx->ret < 0))
// 		return;

// 	/* ignore dynamic shared libraries */
// 	if (strstr((char *)ctx->arg[SYSCALL_ARG0], DLIB_SUFF) == NULL &&
// 		strstr((char *)ctx->arg[SYSCALL_ARG0], DLIB_SUFF_ALT) == NULL) {
// #ifdef DEBUG_PRINT_TRACE
//         logprintf("open syscall: %d %s\n", (int)ctx->ret, (char*)ctx->arg[SYSCALL_ARG0]);
//         //fprintf(stderr, "open syscall: %d %s\n", (int)ctx->ret, (char*)ctx->arg[SYSCALL_ARG0]);
// #endif
//         if (strcmp((char*)ctx->arg[SYSCALL_ARG0], "/etc/localtime") == 0) {
// #ifdef DEBUG_PRINT_TRACE
//             logprintf("localtime, ignored due to address randomization!\n");
// #endif
//             return;
//         }

// 		if (filename_predicate == NULL)
// 			fdset.insert((int)ctx->ret);
// 		else if (filename_predicate((char*)ctx->arg[SYSCALL_ARG0]))
// 			fdset.insert((int)ctx->ret);
//     }
// #else
//   /* not successful; optimized branch */                                         
//   if (unlikely((long)ctx->ret < 0))                                              
//     return; 
// 	//mf: TODO implement if necessary
//   logprintf("[open syscall] fd is %lu and file name is %s\n", ctx->ret, (char*) ctx->arg[SYSCALL_ARG0]);
// #endif
}

/*
 * auxiliary (helper) function
 *
 * whenever close(2) is invoked, check
 * the descriptor and remove if it was
 * inside the monitored set of descriptors
 */
void
post_close_hook(syscall_ctx_t *ctx)
{
// #ifndef USE_CUSTOM_TAG
// 	/* iterator */
// 	set<int>::iterator it;

// 	/* not successful; optimized branch */
// 	if (unlikely((long)ctx->ret < 0))
// 		return;

	
// 	 * if the descriptor (argument) is
// 	 * interesting, remove it from the
// 	 * monitored set
	 
// 	it = fdset.find((int)ctx->arg[SYSCALL_ARG0]);
// 	if (likely(it != fdset.end()))
// 		fdset.erase(it);
// #else
// 	//mf: TODO implement
// #endif
}











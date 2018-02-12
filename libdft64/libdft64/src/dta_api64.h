#pragma once
#include <sys/socket.h>
#include <unistd.h>
#include "pin.H"

typedef bool (*FILENAME_PREDICATE_CALLBACK)(char*);
typedef bool (*NETWORK_PREDICATE_CALLBACK)(struct sockaddr*, uint32_t);

// Insert sink checks to assume that no control flow statment will be influence
// by the tainted bytes. Will replace ins_desc for:
// XED_ICLASS_CALL_NEAR
// XED_ICLASS_JMP
// XED_ICLASS_RET_NEAR
//
// The passed function pointer has the signature void (ADDRINT ins, ADDRINT bt), where
// @ins:    address of the offending instruction
// @bt:     address of the branch target
int dta_sink_control_flow(AFUNPTR, AFUNPTR);

// Add network + file as sources. Will replace sys_desc for:
//  __NR_read
//  __NR_readv
//  __NR_dup
//  __NR_dup2
//  __NR_close
//  __NR_open
//  __NR_creat
//  __NR_socket
//  __NR_recv
//  __NR_accept
//  __NR_accept4
//  __NR_recvfrom
//  __NR_recvmsg
//
// track_file: whether we track file I/O
// FILENAME_PREDICATE_CALLBACK: a callback for deciding whether to track each
//  file. NULL means track all file
// track_network: whether we track network I/O
// NETWORK_PREDICATE_CALLBACK: a callback for deciding whether to track each
//  connection. NULL means track all connections.
// track_stdin: whether we track things from std input
int dta_source(bool track_file, FILENAME_PREDICATE_CALLBACK, bool track_network,
                    NETWORK_PREDICATE_CALLBACK /*ignored*/, bool track_stdin);

// test whether a fd value is currently being tracked
bool dta_is_fd_interesting(int);

//dtracker
void post_open_hook(syscall_ctx_t *ctx);
void post_read_hook(syscall_ctx_t *ctx);
void post_write_hook(syscall_ctx_t *ctx);
void post_close_hook(syscall_ctx_t *ctx);

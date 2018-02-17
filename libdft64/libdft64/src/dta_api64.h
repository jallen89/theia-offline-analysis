#pragma once
#include <sys/socket.h>
#include <unistd.h>
#include "pin.H"

typedef bool (*FILENAME_PREDICATE_CALLBACK)(char*);
typedef bool (*NETWORK_PREDICATE_CALLBACK)(struct sockaddr*, uint32_t);

//dtracker
void post_open_hook(syscall_ctx_t *ctx);
void post_read_hook(syscall_ctx_t *ctx);
void post_write_hook(syscall_ctx_t *ctx);
void post_close_hook(syscall_ctx_t *ctx);

/*-
 * Copyright (c) 2010, 2011, 2012, 2013, Columbia University
 * All rights reserved.
 *
 * This software was developed by Vasileios P. Kemerlis <vpk@cs.columbia.edu>
 * at Columbia University, New York, NY, USA, in June 2010.
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
 * 02/23/2011:
 * 	some conflict arises when numaif.h is included before syscall_desc.h
 * 	the proposed fix was done by Georgios Portokalidis
 * 	(porto@cs.columbia.edu)
 */

/*
 * TODO:
 * 	- add ioctl() handler
 * 	- add nfsservctl() handler
 */

#include <sys/epoll.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <asm/prctl.h>

#include <asm/fcntl.h>
//#include <asm/stat.h>
#include <linux/sysctl.h>

#include <err.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#include "syscall_desc64.h"
#include "tagmap64.h"
#include <linux/mempolicy.h>

/* callbacks declaration */
static void post_read_hook(syscall_ctx_t*);
static void post_fcntl_hook(syscall_ctx_t*);
static void post_mmap_hook(syscall_ctx_t*);
static void post_syslog_hook(syscall_ctx_t*);
static void post_modify_ldt_hook(syscall_ctx_t*);
static void post_quotactl_hook(syscall_ctx_t *ctx);
static void post_readv_hook(syscall_ctx_t*);
static void post__sysctl_hook(syscall_ctx_t*);
static void post_poll_hook(syscall_ctx_t *ctx);
static void post_rt_sigpending_hook(syscall_ctx_t *ctx);
static void post_getcwd_hook(syscall_ctx_t *ctx);
static void post_getgroups_hook(syscall_ctx_t*);
static void post_mincore_hook(syscall_ctx_t *ctx);
static void post_getdents_hook(syscall_ctx_t *ctx);
static void post_getxattr_hook(syscall_ctx_t *ctx);
static void post_listxattr_hook(syscall_ctx_t *ctx);
static void post_io_getevents_hook(syscall_ctx_t *ctx);
static void post_get_mempolicy_hook(syscall_ctx_t *ctx);
static void post_lookup_dcookie_hook(syscall_ctx_t *ctx);
static void post_mq_timedreceive_hook(syscall_ctx_t *ctx);
static void post_readlinkat_hook(syscall_ctx_t*);
static void post_epoll_wait_hook(syscall_ctx_t *ctx);
static void post_arch_prctl_hook(syscall_ctx_t *ctx);
static void post_accept_accept4_getsockname_getpeername_hook(syscall_ctx_t *ctx);
static void post_shmctl_hook(syscall_ctx_t *ctx);
static void post_recvfrom_hook(syscall_ctx_t *ctx);
static void post_recvmsg_hook(syscall_ctx_t *ctx);
static void post_getsockopt_hook(syscall_ctx_t *ctx);
static void post_msgctl_hook(syscall_ctx_t *ctx);
static void post_msgrcv_hook(syscall_ctx_t *ctx);
static void post_semctl_hook(syscall_ctx_t *ctx);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
static void post_sendmmsg_hook(syscall_ctx_t *ctx);
static void post_recvmmsg_hook(syscall_ctx_t *ctx);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
static void post_process_vm_readv_hook(syscall_ctx_t *ctx);
#endif

/* syscall descriptors */
syscall_desc_t syscall_desc[SYSCALL_MAX] = {
	/* __NR_read */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_read_hook },
	/* __NR_write */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_open */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_close */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_stat XXX stat struct not sure */
	{ 2, 0, 1, { 0, sizeof(struct stat), 0, 0, 0, 0 }, NULL,
	NULL },
	/* __NR_fstat XXX stat struct */
	{ 2, 0, 1, { 0, sizeof(struct stat), 0, 0, 0, 0 }, NULL,
	NULL },
	/* __NR_lstat XXX stat struct */
	{ 2, 0, 1, { 0, sizeof(struct stat), 0, 0, 0, 0 }, NULL,
	NULL },
	/* __NR_poll */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_poll_hook },
	/* __NR_lseek */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mmap */
	{ 6, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_mmap_hook },
	/* __NR_mprotect */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_munmap */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_brk */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_rt_sigaction */
	{ 4, 0, 1, { 0, 0, sizeof(struct sigaction), 0, 0, 0 }, NULL, NULL },
	/* __NR_rt_sigprocmask */
	{ 4, 0, 1, { 0, 0, sizeof(sigset_t), 0, 0, 0 }, NULL, NULL },
	/* __NR_rt_sigreturn */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ioctl; TODO */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_pread64 */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_read_hook },
	/* __NR_pwrite64 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_readv */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_readv_hook },
	/* __NR_writev */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_access */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_pipe */
	{ 1, 0, 1, { sizeof(int) * 2, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_select */
	{ 5, 0, 1, { 0, sizeof(fd_set), sizeof(fd_set), sizeof(fd_set),
	sizeof(struct timeval), 0 }, NULL, NULL },
	/* __NR_sched_yield */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mremap */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_msync */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mincore */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_mincore_hook },
	/* __NR_madvise */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* FIXME: __NR_shmget; shared memeory, ignores for now */
    { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* FIXME: __NR_shmat; shared memory, ignores for now */
    { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* FIXME: __NR_shmctl; shared memory, ignores for now */
    { 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_shmctl_hook },
	/* __NR_dup */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_dup2 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_pause */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_nanosleep */
	{ 2, 0, 1, { 0, sizeof(struct timespec), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getitimer */
	{ 2, 0, 1, { 0, sizeof(struct itimerval), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_alarm */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setitimer */
	{ 3, 0, 1, { 0, 0, sizeof(struct itimerval), 0, 0, 0 }, NULL, NULL },
	/* __NR_getpid */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sendfile */
	{ 4, 0, 1, { 0, 0, sizeof(off_t), 0, 0, 0 }, NULL, NULL },
    /* __NR_socket; create a socket, returns fd */
    { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_connect */
    { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_accept */
    { 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_accept_accept4_getsockname_getpeername_hook },
    /* __NR_sendto */
    { 6, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL},
    /* __NR_recvfrom */
    { 6, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_recvfrom_hook },
    /* __NR_sendmsg */
    { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL},
    /* __NR_recvmsg */
    { 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_recvmsg_hook },
    /* __NR_shutdown */
    { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_bind */
    { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_listen */
    { 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_getsockname */
    { 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_accept_accept4_getsockname_getpeername_hook },
    /* __NR_getpeername */
    { 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_accept_accept4_getsockname_getpeername_hook },
    /* __NR_socketpair */
    { 4, 0, 1, { 0, 0, 0, sizeof(int) * 2, 0, 0 }, NULL, NULL},
    /* __NR_setsockopt */
    { 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_getsockopt */
    { 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getsockopt_hook },
	/* __NR_clone */
	{ 5, 0, 1, { 0, 0, sizeof(int), 0, 0, 0 }, NULL, NULL },
	/* __NR_fork */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_vfork */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_execve */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_exit */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_wait4 */
	{ 4, 0, 1, { 0, sizeof(int), 0, sizeof(struct rusage), 0, 0 },
	NULL, NULL },
	/* __NR_kill */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_uname */
	{ 1, 0, 1, { sizeof(struct new_utsname), 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_semget */
    { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_semop  */
    { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_semctl */
    { 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_semctl_hook },
    /* FIXME: __NR_shmdt; shared memory, ignored for now */
    { 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL},
    /* __NR_msgget */
    { 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL},
    /* __NR_msgsnd */
    { 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL},
    /* __NR_msgrcv */
    { 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_msgrcv_hook },
    /* __NR_msgctl */
    { 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_msgctl_hook },
	/* __NR_fcntl */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_fcntl_hook },
	/* __NR_flock */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fsync */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fdatasync */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_truncate */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ftruncate */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getdents */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getdents_hook },
	/* __NR_getcwd */
	{ 2, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getcwd_hook },
	/* __NR_chdir */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fchdir */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_rename */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mkdir */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_rmdir */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_creat */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_link */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_unlink */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_symlink */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_readlink */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_read_hook },
	/* __NR_chmod */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fchmod */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_chown */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fchown */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_lchown */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_umask */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_gettimeofday */
	{ 2, 0, 1, { sizeof(struct timeval), sizeof(struct timezone), 0,
	 0, 0, 0 }, NULL, NULL },
	/* __NR_getrlimit */
	{ 2, 0, 1, { 0, sizeof(struct rlimit), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getrusage */
	{ 2, 0, 1, { 0, sizeof(struct rusage), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sysinfo */
	{ 1, 0, 1, { sizeof(struct sysinfo), 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_times */
	{ 1, 0, 1, { sizeof(struct tms), 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ptrace */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getuid */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_syslog */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_syslog_hook },
	/* __NR_getgid */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setuid */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setgid */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_geteuid */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getegid */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setpgid */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getppid */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getpgrp */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setsid */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setreuid */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setregid */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getgroups */
	{ 2, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getgroups_hook },
	/* __NR_setgroups */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setresuid */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getresuid */
	{ 3, 0, 1, { sizeof(uid_t), sizeof(uid_t), sizeof(uid_t), 0, 0, 0 },
	NULL, NULL },
	/* __NR_setresgid */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getresgid */
	{ 3, 0, 1, { sizeof(gid_t), sizeof(gid_t), sizeof(gid_t), 0, 0, 0 },
	NULL, NULL },
	/* __NR_getpgid */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setfsuid */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setfsgid */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getsid */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_capget */
	{ 2, 0, 1, { sizeof(cap_user_header_t), sizeof(cap_user_data_t), 0, 0,
	0, 0 }, NULL, NULL },
	/* __NR_capset */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_rt_sigpending */
	{ 2, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_rt_sigpending_hook },
	/* __NR_rt_sigtimedwait */
	{ 4, 0, 1, { 0, sizeof(siginfo_t), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_rt_sigqueueinfo */
	{ 3, 0, 1, { 0, 0, sizeof(siginfo_t), 0, 0, 0 }, NULL, NULL },
	/* __NR_rt_sigsuspend */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sigaltstack */
	{ 2, 0, 1, { 0, sizeof(stack_t), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_utime */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mknod */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_uselib */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_personality */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ustat */
	{ 2, 0, 1, { 0, sizeof(struct ustat), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_statfs */
	{ 2, 0, 1, { 0, sizeof(struct statfs), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fstatfs */
	{ 2, 0, 1, { 0, sizeof(struct statfs), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sysfs */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getpriority */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setpriority */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_setparam */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_getparam */
	{ 2, 0, 1, { 0, sizeof(struct sched_param), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_setscheduler */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_sched_getscheduler */
    { 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_get_priority_max */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_get_priority_min */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_rr_get_interval */
	{ 2, 0, 1, { 0, sizeof(struct timespec), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mlock */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_munlock */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mlockall */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_munlockall */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_vhangup */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_modify_ldt */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_modify_ldt_hook },
	/* __NR_pivot_root */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR__sysctl */
	{ 1, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post__sysctl_hook },
	/* __NR_prctl */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_arch_prctl */
    { 2, 1, 0, { 0, 0, 0, 0, 0, 0}, NULL, post_arch_prctl_hook },
	/* __NR_adjtimex */
	{ 1, 0, 1, { sizeof(struct timex), 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setrlimit */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_chroot */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sync */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_acct */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_settimeofday */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mount */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_umount2 */
    { 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_swapon */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_swapoff */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_reboot */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sethostname */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setdomainname */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_iopl */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ioperm */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_create_module; not implemented */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_init_module */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_delete_module */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_get_kernel_syms; not implemented */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_query_module; not implemented */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_quotactl */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_quotactl_hook },
	/* __NR_nfsservctl; TODO */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_getpmsg */
    { 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_putpmsg */
    { 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL},
	/* __NR_afs_syscall; not implemented in linux XXX */
    { 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_tuxcall; not implement in linux XXX */
    { 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_security; not implement in linux XXX */
    { 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_gettid */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_readahead */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setxattr */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_lsetxattr */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fsetxattr */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getxattr */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getxattr_hook },
	/* __NR_lgetxattr */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getxattr_hook },
	/* __NR_fgetxattr */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getxattr_hook },
	/* __NR_listxattr */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_listxattr_hook },
	/* __NR_llistxattr */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_listxattr_hook },
	/* __NR_flistxattr */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_listxattr_hook },
	/* __NR_removexattr */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_lremovexattr */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fremovexattr */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_tkill */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_time */
	{ 1, 0, 1, { sizeof(time_t), 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_futex */
	{ 6, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_setaffinity */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_getaffinity */
	{ 3, 0, 1, { 0, 0, sizeof(cpu_set_t), 0, 0, 0 }, NULL, NULL },
	/* __NR_set_thread_area */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_io_setup */
	{ 2, 0, 1, { 0, sizeof(aio_context_t), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_io_destroy */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_io_getevents */
	{ 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_io_getevents_hook },
	/* __NR_io_submit */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_io_cancel */
	{ 3, 0, 1, { 0, 0, sizeof(struct io_event), 0, 0, 0 }, NULL, NULL },
	/* __NR_get_thread_area */
	{ 1, 0, 1, { sizeof(struct user_desc), 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_lookup_dcookie */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_lookup_dcookie_hook },
	/* __NR_epoll_create */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_epoll_ctl_old; FIXME */
    { 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_epoll_wait_old */
    { 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_epoll_wait_hook },
	/* __NR_remap_file_pages */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_getdents64; XXX here we reuse the getdents handler */
    { 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getdents_hook},
	/* __NR_set_tid_address */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_restart_syscall */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_semtimedop */
    { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fadvise64 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_timer_create */
	{ 3, 0, 1, { 0, 0, sizeof(timer_t), 0, 0, 0 }, NULL, NULL },
	/* __NR_timer_settime */
	{ 4, 0, 1, { 0, 0, 0, sizeof(struct itimerspec), 0, 0 }, NULL, NULL },
	/* __NR_timer_gettime */
	{ 2, 0, 1, { 0, sizeof(struct itimerspec), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_timer_getoverrun */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_timer_delete */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_clock_settime */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_clock_gettime */
	{ 2, 0, 1, { 0, sizeof(struct timespec), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_clock_getres */
	{ 2, 0, 1, { 0, sizeof(struct timespec), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_clock_nanosleep */
	{ 4, 0, 1, { 0, 0, 0, sizeof(struct timespec), 0, 0 }, NULL, NULL },
	/* __NR_exit_group */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_epoll_wait */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_epoll_wait_hook },
	/* __NR_epoll_ctl; FIXME */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_tgkill */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_utimes */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_vserver; not implemented */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mbind */
	{ 6, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_set_mempolicy */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_get_mempolicy */
	{ 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_get_mempolicy_hook },
	/* __NR_mq_open */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mq_unlink */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mq_timedsend */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mq_timedreceive */
	{ 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_mq_timedreceive_hook },
	/* __NR_mq_notify */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mq_getsetattr */
	{ 3, 0, 1, { 0, 0, sizeof(struct mq_attr), 0, 0, 0 }, NULL, NULL },
	/* __NR_kexec_load */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_waitid */
	{ 4, 0, 1, { 0, 0, sizeof(siginfo_t), 0, sizeof(struct rusage), 0 },
	NULL, NULL },
	/* __NR_add_key */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_request_key */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_keyctl */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ioprio_set */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ioprio_get */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_inotify_init */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_inotify_add_watch */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_inotify_rm_watch */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_migrate_pages */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_openat */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mkdirat */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mknodat */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fchownat */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_futimesat */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_newfstatat XXX struct stat*/
    { 4, 1, 0, { 0, 0, sizeof(struct stat), 0, 0, 0 }, NULL, NULL },
	/* __NR_unlinkat */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_renameat */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_linkat */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_symlinkat */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_readlinkat */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_readlinkat_hook },
	/* __NR_fchmodat */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_faccessat */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_pselect6 */
	{ 6, 0, 1, { 0, sizeof(fd_set), sizeof(fd_set), sizeof(fd_set), 0, 0 },
	NULL, NULL },
	/* __NR_ppoll */
	{ 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_poll_hook },
	/* __NR_unshare */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_set_robust_list */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_get_robust_list */
	{ 3, 0, 1, { 0, sizeof(struct robust_list_head*), sizeof(size_t), 0, 0,
	0 }, NULL, NULL },
	/* __NR_splice */
	{ 6, 0, 1, { 0, sizeof(loff_t), 0, sizeof(loff_t), 0, 0 }, NULL, NULL },
	/* __NR_tee */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sync_file_range */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_vmsplice */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_move_pages */
	{ 6, 0, 1, { 0, 0, 0, 0, sizeof(int), 0 }, NULL, NULL },
	/* __NR_utimensat */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_epoll_pwait */
	{ 6, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_epoll_wait_hook },
	/* __NR_signalfd */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_timerfd_create */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_eventfd */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fallocate */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_timerfd_settime */
	{ 4, 0, 1, { 0, 0, 0, sizeof(struct itimerspec), 0, 0 }, NULL, NULL },
	/* __NR_timerfd_gettime */
	{ 2, 0, 1, { 0, sizeof(struct itimerspec), 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_accept4 */
    { 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_accept_accept4_getsockname_getpeername_hook },
	/* __NR_signalfd4 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_eventfd2 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_epoll_create1 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_dup3 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_pipe2 */
	{ 2, 0, 1, { sizeof(int) * 2, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_inotify_init1 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_preadv */
	{ 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_readv_hook },
	/* __NR_pwritev */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_rt_tgsigqueueinfo */
	{ 4, 0, 1, { 0, 0, 0, sizeof(siginfo_t), 0, 0 }, NULL, NULL },
	/* __NR_perf_event_open */
	{ 5, 0, 1, { sizeof(struct perf_event_attr), 0, 0, 0, 0, 0 }, NULL,
	NULL }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
	/* __NR_recvmmsg */
	,{ 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_recvmmsg_hook },
	/* __NR_fanotify_init */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fanotify_mark */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_prlimit64 */
    { 4, 0, 1, { 0, 0, 0, sizeof(struct rlimit), 0, 0}, NULL, NULL},
	/* __NR_name_to_handle_at */
	{ 5, 0, 1, { 0, 0, sizeof(struct file_handle_libdft), sizeof(int), 0, 0 },
	NULL, NULL },
	/* __NR_open_by_handle_at */
	{ 3, 0, 1, { 0, sizeof(struct file_handle_libdft), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_clock_adjtime */
	{ 2, 0, 1, { 0, sizeof(struct timex), 0, 0, 0, 0 }, NULL, NULL },
    /* __NR_syncfs */
    { 1, 0, 0, { 0, 0, 0, 0, 0, 0}, NULL, NULL},
    /* __NR_sendmmsg */
    { 4, 1, 0, { 0, 0, 0, 0, 0, 0}, NULL, post_sendmmsg_hook},
    /* __NR_setns */
    { 2, 0, 0, { 0, 0, 0, 0, 0, 0}, NULL, NULL},
	/* __NR_getcpu */
	{ 3, 0, 1, { sizeof(unsigned), sizeof(unsigned),
	sizeof(struct getcpu_cache), 0, 0, 0 }, NULL, NULL },
    /* __NR_process_vm_readv */
    { 6, 1, 0, { 0, 0, 0, 0, 0, 0}, NULL, post_process_vm_readv_hook},
    /* __NR_process_vm_writev */
    { 6, 0, 0, { 0 ,0, 0, 0, 0, 0}, NULL, NULL}
#endif
};


/*
 * add a new pre-syscall callback into a syscall descriptor
 *
 * @desc:	the syscall descriptor
 * @pre:	function pointer to the pre-syscall handler
 *
 * returns:	0 on success, 1 on error
 */
int
syscall_set_pre(syscall_desc_t *desc, void (* pre)(syscall_ctx_t*))
{
	/* sanity checks; optimized branch */
	if (unlikely((desc == NULL) | (pre == NULL)))
		/* return with failure */
		return 1;

	/* update the pre-syscall callback */
	desc->pre = pre;

	/* set the save arguments flag */
	desc->save_args = 1;

	/* success */
	return 0;
}

/*
 * add a new post-syscall callback into a syscall descriptor
 *
 * @desc:	the syscall descriptor
 * @post:	function pointer to the post-syscall handler
 *
 * returns:	0 on success, 1 on error
 */
int
syscall_set_post(syscall_desc_t *desc, void (* post)(syscall_ctx_t*))
{
	/* sanity checks; optimized branch */
	if (unlikely((desc == NULL) | (post == NULL)))
		/* return with failure */
		return 1;

	/* update the post-syscall callback */
	desc->post = post;

	/* set the save arguments flag */
	desc->save_args = 1;

	/* success */
	return 0;
}

/*
 * remove the pre-syscall callback from a syscall descriptor
 *
 * @desc:       the syscall descriptor
 *
 * returns:     0 on success, 1 on error
 */
int
syscall_clr_pre(syscall_desc_t *desc)
{
	/* sanity check; optimized branch */
	if (unlikely(desc == NULL))
		/* return with failure */
		return 1;

	/* clear the pre-syscall callback */
	desc->pre = NULL;

	/* check if we need to clear the save arguments flag */
	if (desc->post == NULL)
		/* clear */
		desc->save_args = 0;

	/* return with success */
	return 0;
}

/*
 * remove the post-syscall callback from a syscall descriptor
 *
 * @desc:       the syscall descriptor
 *
 * returns:     0 on success, 1 on error
 */
int
syscall_clr_post(syscall_desc_t *desc)
{
	/* sanity check; optimized branch */
	if (unlikely(desc == NULL))
		/* return with failure */
		return 1;

	/* clear the post-syscall callback */
	desc->post = NULL;

	/* check if we need to clear the save arguments flag */
	if (desc->pre == NULL)
		/* clear */
		desc->save_args = 0;

	/* return with success */
	return 0;
}

/* __NR_(p)read(64) and __NR_readlink post syscall hook */
static void
post_read_hook(syscall_ctx_t *ctx)
{
	/* read()/readlink() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
}

/* __NR_getgroups post syscall_hook */
static void
post_getgroups_hook(syscall_ctx_t *ctx)
{
	/* getgroups() was not successful */
	if ((long)ctx->ret <= 0 || (gid_t *)ctx->arg[SYSCALL_ARG1] == NULL)
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1],
			(sizeof(gid_t) * (size_t)ctx->ret));
}

/* __NR_readlinkat post syscall hook */
static void
post_readlinkat_hook(syscall_ctx_t *ctx)
{
	/* readlinkat() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG2], (size_t)ctx->ret);
}

/* __NR_mmap post syscall hook */
static void
post_mmap_hook(syscall_ctx_t *ctx)
{
	/* the map offset */
	size_t offset = (size_t)ctx->arg[SYSCALL_ARG1];

	/* mmap() was not successful; optimized branch */
	if (unlikely((void *)ctx->ret == MAP_FAILED))
		return;

	/* estimate offset; optimized branch */
	if (unlikely(offset < PAGE_SZ))
		offset = PAGE_SZ;
	else
		offset = offset + PAGE_SZ - (offset % PAGE_SZ);

	/* grow downwards; optimized branch */
	if (unlikely((int)ctx->arg[SYSCALL_ARG3] & MAP_GROWSDOWN))
		/* fix starting address */
		ctx->ret = ctx->ret - offset;

	/* emulate the clear_tag() call */
	tagmap_clrn((size_t)ctx->ret, offset);
}

/* __NR_readv and __NR_preadv post syscall hook */
static void
post_readv_hook(syscall_ctx_t *ctx)
{
	/* iterators */
	int	i;
	struct	iovec *iov;

	/* bytes copied in a iovec structure */
	size_t	iov_tot;

	/* total bytes copied */
	size_t	tot = (size_t)ctx->ret;

	/* (p)readv() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* iterate the iovec structures */
	for (i = 0; i < (int)ctx->arg[SYSCALL_ARG2] && tot > 0; i++) {
		/* get an iovec  */
		iov = ((struct iovec *)ctx->arg[SYSCALL_ARG1]) + i;

		/* get the length of the iovec */
		iov_tot = (tot >= (size_t)iov->iov_len) ?
				(size_t)iov->iov_len : tot;

		/* clear the tag bits */
		tagmap_clrn((size_t)iov->iov_base, iov_tot);

		/* housekeeping */
		tot -= iov_tot;
	}
}

/* __NR_epoll_pwait post syscall hook */
static void
post_epoll_wait_hook(syscall_ctx_t *ctx)
{

	/* epoll_pwait() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1],
			sizeof(struct epoll_event) * (size_t)ctx->ret);
}

/* __NR_poll and __NR_ppoll post syscall hook */
static void
post_poll_hook(syscall_ctx_t *ctx)
{
	/* iterators */
	size_t	i;
	struct	pollfd *pfd;

	/* (p)poll() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* iterate the pollfd structures */
	for (i = 0; i < (size_t)ctx->arg[SYSCALL_ARG1]; i++) {
		/* get pollfd */
		pfd = ((struct pollfd *)ctx->arg[SYSCALL_ARG0]) + i;

		/* clear the tag bits */
		tagmap_clrn((size_t)&pfd->revents, sizeof(short));
	}
}

/* __NR_mq_timedreceive post syscall hook */
static void
post_mq_timedreceive_hook(syscall_ctx_t *ctx)
{
	/* mq_timedreceive() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);

	/* priority argument is supplied */
	if ((size_t *)ctx->arg[SYSCALL_ARG3] != NULL)
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG3], sizeof(size_t));
}

/* __NR_get_mempolicy */
static void
post_get_mempolicy_hook(syscall_ctx_t *ctx)
{
	/* get_mempolicy() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	/* flags is zero */
	if ((unsigned long)ctx->arg[SYSCALL_ARG4] == 0) {
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG0], sizeof(int));
		tagmap_clrn(ctx->arg[SYSCALL_ARG1],
						sizeof(unsigned long));
		/* done */
		return;
	}

	/* MPOL_F_MEMS_ALLOWED is set on flags */
	if (((unsigned long)ctx->arg[SYSCALL_ARG4] &
				MPOL_F_MEMS_ALLOWED) != 0) {
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG1],
						sizeof(unsigned long));
		/* done */
		return;
	}

	/* MPOL_F_ADDR is set on flags */
	if (((unsigned long)ctx->arg[SYSCALL_ARG4] & MPOL_F_ADDR) != 0 &&
		((unsigned long)ctx->arg[SYSCALL_ARG4] & MPOL_F_NODE) == 0) {
		/* mode is provided */
		if ((int *)ctx->arg[SYSCALL_ARG0] != NULL)
			/* clear the tag bits */
			tagmap_clrn(ctx->arg[SYSCALL_ARG0],
							sizeof(int));

		/* nodemask is provided */
		if ((unsigned long *)ctx->arg[SYSCALL_ARG1] != NULL)
			/* clear the tag bits */
			tagmap_clrn(ctx->arg[SYSCALL_ARG1],
						sizeof(unsigned long));
		/* done */
		return;
	}

	/* MPOL_F_NODE & MPOL_F_ADDR is set on flags */
	if (((unsigned long)ctx->arg[SYSCALL_ARG4] & MPOL_F_ADDR) != 0 &&
		((unsigned long)ctx->arg[SYSCALL_ARG4] & MPOL_F_NODE) != 0) {
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG0], sizeof(int));
		/* done */
		return;
	}

	/* MPOL_F_NODE is set on flags */
	if (((unsigned long)ctx->arg[SYSCALL_ARG4] & MPOL_F_NODE) != 0) {
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG0], sizeof(int));
		/* done */
		return;
	}
}

/* __NR_lookup_dcookie post syscall hook */
static void
post_lookup_dcookie_hook(syscall_ctx_t *ctx)
{
	/* lookup_dcookie() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
}

/* __NR_io_getevents post syscall hook */
static void
post_io_getevents_hook(syscall_ctx_t *ctx)
{
	/* io_getevents() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG3],
				sizeof(struct io_event) * (size_t)ctx->ret);

	/* timespec is specified */
	if ((struct timespec *)ctx->arg[SYSCALL_ARG4] != NULL)
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG4],
						sizeof(struct timespec));
}

/* __NR_(f, l)listxattr post syscall hook */
static void
post_listxattr_hook(syscall_ctx_t *ctx)
{
	/* *listxattr() was not successful; optimized branch */
	if ((long)ctx->ret <= 0 || (void *)ctx->arg[SYSCALL_ARG1] == NULL)
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
}

/* __NR_(f, l)getxattr post syscall hook */
static void
post_getxattr_hook(syscall_ctx_t *ctx)
{
	/* *getxattr() was not successful; optimized branch */
	if ((long)ctx->ret <= 0 || (void *)ctx->arg[SYSCALL_ARG2] == NULL)
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG2], (size_t)ctx->ret);
}

/* __NR_getdents post syscall hook */
static void
post_getdents_hook(syscall_ctx_t *ctx)
{
	/* getdents() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
}

/* __NR_mincore post syscall hook */
static void
post_mincore_hook(syscall_ctx_t *ctx)
{
	/* mincore() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG2],
		(((size_t)ctx->arg[SYSCALL_ARG1] + PAGE_SZ - 1) / PAGE_SZ));
}

/* __NR_shmctl post syscall hook */
static void
post_shmctl_hook(syscall_ctx_t *ctx)
{
    /* shmctl() was not successful; optimized branch */
    if (unlikely((long)ctx->ret < 0))
        return;

    /* fix the cmd parameter */
    if (ctx->arg[SYSCALL_ARG1] > IPC_FIX)
        ctx->arg[SYSCALL_ARG1] -= IPC_FIX;

    /* differentiate based on the cmd */
    switch ((int)ctx->arg[SYSCALL_ARG1]) {
        case IPC_STAT:
        case SHM_STAT:
            /* clear the tag bits */
            tagmap_clrn(ctx->arg[SYSCALL_ARG2],
                sizeof(struct shmid_ds));
            break;
        case IPC_INFO:
        case SHM_INFO:
            /* clear the tag bits */
            tagmap_clrn(ctx->arg[SYSCALL_ARG2],
                sizeof(struct shminfo));
            break;
        default:
            /* nothing to do */
            return;
    }
}

/* __NR_getcwd post syscall hook */
static void
post_getcwd_hook(syscall_ctx_t *ctx)
{
	/* getcwd() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG0], (size_t)ctx->ret);
}

/* __NR_rt_sigpending post syscall hook */
static void
post_rt_sigpending_hook(syscall_ctx_t *ctx)
{
	/* rt_sigpending() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG0], (size_t)ctx->arg[SYSCALL_ARG1]);
}

/* __NR_quotactl post syscall hook */
static void
post_quotactl_hook(syscall_ctx_t *ctx)
{
	/* offset */
	size_t off;

	/* quotactl() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	/* different offset ranges */
	switch ((int)ctx->arg[SYSCALL_ARG0]) {
		case Q_GETFMT:
			off = sizeof(__u32);
			break;
		case Q_GETINFO:
			off = sizeof(struct if_dqinfo);
			break;
		case Q_GETQUOTA:
			off = sizeof(struct if_dqblk);
			break;
		case Q_XGETQSTAT:
			off = sizeof(struct fs_quota_stat);
			break;
		case Q_XGETQUOTA:
			off = sizeof(struct fs_disk_quota);
			break;
		default:
			/* nothing to do */
			return;
	}

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG3], off);
}

/* __NR_modify_ldt post syscall hook */
static void
post_modify_ldt_hook(syscall_ctx_t *ctx)
{
	/* modify_ldt() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
}

static void
post_accept_accept4_getsockname_getpeername_hook(syscall_ctx_t *ctx)
{
    /* not successful; optimized branch */
    if (unlikely((long)ctx->ret < 0))
        return;

    /* addr argument is provided */
    if ((void *)ctx->arg[SYSCALL_ARG1] != NULL) {
        /* clear the tag bits */
        tagmap_clrn(ctx->arg[SYSCALL_ARG1],
            *((socklen_t *)ctx->arg[SYSCALL_ARG2]));

        /* clear the tag bits */
        tagmap_clrn(ctx->arg[SYSCALL_ARG2], sizeof(socklen_t));
    }
}

static void
post_recvmsg_hook(syscall_ctx_t *ctx)
{
    /* not successful; optimized branch */
    if (unlikely((long)ctx->ret <= 0))
        return;

    /* extract the message header */
    struct msghdr *msg = (struct msghdr *)ctx->arg[SYSCALL_ARG1];

    /* source address specified */
    if (msg->msg_name != NULL) {
        /* clear the tag bits */
        tagmap_clrn((size_t)msg->msg_name,
            msg->msg_namelen);

        /* clear the tag bits */
        tagmap_clrn((size_t)&msg->msg_namelen,
                sizeof(socklen_t));
    }

    /* ancillary data specified */
    if (msg->msg_control != NULL) {
        /* clear the tag bits */
        tagmap_clrn((size_t)msg->msg_control,
            msg->msg_controllen);

        /* clear the tag bits */
        tagmap_clrn((size_t)&msg->msg_controllen,
                sizeof(size_t));
    }

    /* flags; clear the tag bits */
    tagmap_clrn((size_t)&msg->msg_flags, sizeof(int));

    /* total bytes received */
    size_t tot = (size_t)ctx->ret;

    /* iterate the iovec structures */
    for (size_t i = 0; i < msg->msg_iovlen && tot > 0; i++) {
        /* get the next I/O vector */
        struct iovec *iov = &msg->msg_iov[i];

        /* get the length of the iovec */
        size_t iov_tot = (tot > (size_t)iov->iov_len) ?
                (size_t)iov->iov_len : tot;

        /* clear the tag bits */
        tagmap_clrn((size_t)iov->iov_base, iov_tot);

        /* housekeeping */
        tot -= iov_tot;
    }
}

static void
post_semctl_hook(syscall_ctx_t *ctx)
{
    /* semctl() was not successful; optimized branch */
    if (unlikely((long)ctx->ret < 0))
        return;

    /* get the semun structure */
    union semun *su = (union semun *)ctx->arg[SYSCALL_ARG3];

    /* fix the cmd parameter */
    if (ctx->arg[SYSCALL_ARG2] > IPC_FIX)
        ctx->arg[SYSCALL_ARG2] -= IPC_FIX;

    /* differentiate based on the cmd */
    switch ((int)ctx->arg[SYSCALL_ARG2]) {
        case IPC_STAT:
        case SEM_STAT:
            /* clear the tag bits */
            tagmap_clrn((size_t)su->buf,
                sizeof(struct semid_ds));
            break;
        case IPC_INFO:
        case SEM_INFO:
            /* clear the tag bits */
            tagmap_clrn((size_t)su->buf,
                sizeof(struct seminfo));
            break;
        default:
            /* nothing to do */
            return;
    }
}

static void
post_msgrcv_hook(syscall_ctx_t *ctx)
{
    /* msgrcv() was not successful; optimized branch */
    if (unlikely((long)ctx->ret <= 0))
        return;

    /* clear the tag bits */
    tagmap_clrn(ctx->arg[SYSCALL_ARG1],
            (size_t)ctx->ret + sizeof(long));
}

static void
post_msgctl_hook(syscall_ctx_t *ctx)
{
    /* msgctl() was not successful; optimized branch */
    if (unlikely((long)ctx->ret < 0))
        return;

    /* fix the cmd parameter */
    if (ctx->arg[SYSCALL_ARG1] > IPC_FIX)
        ctx->arg[SYSCALL_ARG1] -= IPC_FIX;

    /* differentiate based on the cmd */
    switch ((int)ctx->arg[SYSCALL_ARG1]) {
        case IPC_STAT:
        case MSG_STAT:
            /* clear the tag bits */
            tagmap_clrn(ctx->arg[SYSCALL_ARG2],
                sizeof(struct msqid_ds));
            break;
        case IPC_INFO:
        case MSG_INFO:
            /* clear the tag bits */
            tagmap_clrn(ctx->arg[SYSCALL_ARG2],
                sizeof(struct msginfo));
            break;
        default:
            /* nothing to do */
            return;
    }
}

/* __NR_fcntl post syscall hook */
static void
post_fcntl_hook(syscall_ctx_t *ctx)
{
	/* fcntl() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	/* differentiate based on the cmd argument */
	switch((int)ctx->arg[SYSCALL_ARG1]) {
		/* F_GETLK */
		case F_GETLK:
			/* clear the tag bits */
			tagmap_clrn(ctx->arg[SYSCALL_ARG2],
					sizeof(struct flock));
			break;
		/* F_GETLK64 */
		case F_GETLK64:
			/* clear the tag bits */
			tagmap_clrn(ctx->arg[SYSCALL_ARG2],
					sizeof(struct flock64));
			break;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
		/* F_GETOWN_EX */
		case F_GETOWN_EX:
			/* clear the tag bits */
			tagmap_clrn(ctx->arg[SYSCALL_ARG2],
					sizeof(struct f_owner_ex));
			break;
#endif
		default:
			/* nothing to do */
			break;
	}
}

static void
post_recvfrom_hook(syscall_ctx_t *ctx)
{
    /* not successful; optimized branch */
    if (unlikely((long)ctx->ret <= 0))
        return;

    /* clear the tag bits */
    tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);

    /* sockaddr argument is specified */
    if ((void *)ctx->arg[SYSCALL_ARG4] != NULL) {
        /* clear the tag bits */
        tagmap_clrn(ctx->arg[SYSCALL_ARG4],
            *((socklen_t *)ctx->arg[SYSCALL_ARG5]));

        /* clear the tag bits */
        tagmap_clrn(ctx->arg[SYSCALL_ARG5], sizeof(socklen_t));
    }
}

static void
post_getsockopt_hook(syscall_ctx_t *ctx)
{
    /* not successful; optimized branch */
    if (unlikely((long)ctx->ret < 0))
        return;

    /* clear the tag bits */
    tagmap_clrn(ctx->arg[SYSCALL_ARG3],
            *((socklen_t *)ctx->arg[SYSCALL_ARG4]));

    /* clear the tag bits */
    tagmap_clrn(ctx->arg[SYSCALL_ARG4], sizeof(socklen_t));
}

/*
 * __NR_syslog post syscall hook
 *
 * NOTE: this is not related to syslog(3)
 * see klogctl(3)/syslog(2)
 */
static void
post_syslog_hook(syscall_ctx_t *ctx)
{
	/* syslog() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* differentiate based on the type */
	switch ((int)ctx->arg[SYSCALL_ARG0]) {
		case 2:
		case 3:
		case 4:
			/* clear the tag bits */
			tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
			break;
		default:
			/* nothing to do */
			return;
	}
}

/* __NR__sysctl post syscall hook */
static void
post__sysctl_hook(syscall_ctx_t *ctx)
{
	/* _sysctl arguments */
	struct __sysctl_args *sa;

	/* _sysctl() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	/* _sysctl arguments */
	sa = (struct __sysctl_args *)ctx->arg[SYSCALL_ARG0];

	/* clear the tag bits */
	tagmap_clrn((size_t)sa->newval, sa->newlen);

	/* save old value is specified */
	if (sa->oldval != NULL) {
		/* clear the tag bits */
		tagmap_clrn((size_t)sa->oldval, *sa->oldlenp);

		/* clear the tag bits */
		tagmap_clrn((size_t)sa->oldlenp, sizeof(size_t));
	}
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
/* __NR_recvmmsg post syscall hook */
static void
post_recvmmsg_hook(syscall_ctx_t *ctx)
{
	/* message headers; recvmsg(2) recvmmsg(2) */
	struct	mmsghdr *msg;
	struct	msghdr *m;

	/* iov bytes copied; recvmsg(2) */
	size_t	iov_tot;

	/* iterators */
	size_t	i, j;
	struct	iovec *iov;

	/* total bytes received */
	size_t	tot;

	/* recvmmsg() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	/* iterate the mmsghdr structures */
	for (i = 0; i < (size_t)ctx->ret; i++) {
		/* get the next mmsghdr structure */
		msg = ((struct mmsghdr *)ctx->arg[SYSCALL_ARG1]) + i;

		/* extract the message header */
		m = &msg->msg_hdr;

		/* source address specified */
		if (m->msg_name != NULL) {
			/* clear the tag bits */
			tagmap_clrn((size_t)m->msg_name, m->msg_namelen);

			/* clear the tag bits */
			tagmap_clrn((size_t)&m->msg_namelen, sizeof(int));
		}

		/* ancillary data specified */
		if (m->msg_control != NULL) {
			/* clear the tag bits */
			tagmap_clrn((size_t)m->msg_control, m->msg_controllen);

			/* clear the tag bits */
			tagmap_clrn((size_t)&m->msg_controllen, sizeof(int));
		}

		/* flags; clear the tag bits */
		tagmap_clrn((size_t)&m->msg_flags, sizeof(int));

		/* total bytes received; clear the tag bits */
		tot = (size_t)msg->msg_len;
		tagmap_clrn((size_t)&msg->msg_len, sizeof(unsigned));

		/* iterate the iovec structures */
		for (j = 0; j < m->msg_iovlen && tot > 0; j++) {
			/* get the next I/O vector */
			iov = &m->msg_iov[j];

			/* get the length of the iovec */
			iov_tot = (tot > (size_t)iov->iov_len) ?
					(size_t)iov->iov_len : tot;

			/* clear the tag bits */
			tagmap_clrn((size_t)iov->iov_base, iov_tot);

			/* housekeeping */
			tot -= iov_tot;
		}
	}

	/* timespec structure specified */
	if ((struct timespec *)ctx->arg[SYSCALL_ARG4] != NULL);
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG4], sizeof(struct timespec));
}
#endif

static void
post_arch_prctl_hook(syscall_ctx_t *ctx)
{
    if (unlikely(ctx->ret < 0))
        return;

    switch (ctx->arg[SYSCALL_ARG0]) {
        case ARCH_SET_FS:
        case ARCH_SET_GS:
            tagmap_clrn((size_t)(ctx->arg[SYSCALL_ARG1]), sizeof(long));
        default:
            return;
    }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
static void
post_sendmmsg_hook(syscall_ctx_t *ctx)
{
    if (unlikely(ctx->ret < 0))
        return;

    struct mmsghdr *msgvec = (struct mmsghdr*)ctx->arg[SYSCALL_ARG1];

    size_t i = ctx->ret;
    while (i > 0) {
        i--;
        tagmap_clrn(((size_t)msgvec) + sizeof(struct msghdr), sizeof(unsigned int));
        msgvec++;
    }
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
static void
post_process_vm_readv_hook(syscall_ctx_t *ctx)
{
    if (unlikely(ctx->ret < 0))
        return;

    struct iovec *iov = (struct iovec *)ctx->arg[SYSCALL_ARG1];

    size_t tot = ctx->ret;
    while (tot > 0) {
        if (tot > iov->iov_len) {
            tagmap_clrn((size_t)iov->iov_base, iov->iov_len);
            tot-= iov->iov_len;
        }
        else {
            tagmap_clrn((size_t)iov->iov_base, tot);
            tot = 0;
        }
        iov++;
    }
}
#endif

begin;
\c theia1;
commit;

begin;
create table syscalls (pid integer, cmdline varchar(100), syscall integer, timestamp bigint, filename varchar(100), fuuid bigint);
commit;

begin;
create table file_tagging (f_uuid bigint, off_t bigint, size bigint, tag_uuid bigint);
commit;

begin;
create table path_uuid (uuid varchar(50), uuid bigint);
commit;


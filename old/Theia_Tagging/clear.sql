begin;
\c theia1;
commit;

begin;
delete from syscalls;
commit;

begin;
delete from file_tagging;
commit;

begin;
delete from path_uuid;
commit;


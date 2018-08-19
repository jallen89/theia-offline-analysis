import sys
from fcntl import ioctl
from ioctl_opt import IOR
from os import path
import subprocess
import time
import os
import struct
import glob
from ctypes import *
import ctypes
import uuid

from common import *

log = logging.getLogger(__name__)

class ReplayError(Exception):
    pass


def normal_to_yang_uuid(n_uuid):
    """Converts yang's uuid to a normal uuid.
    yang - 240 9 156 0 0 0 0 0 0 0 0 0 0 0 0 32
    normal - f0099c00-0000-0000-0000-000000000020
    """
    normal = uuid.UUID(n_uuid) if type(str(n_uuid)) is str else n_uuid
    return ' '.join([str(ord(b)) for b in normal.bytes])


def yang_to_normal_uuid(yang_uuid):
    """Converts yang's uuid to a normal uuid."""
    return uuid.UUID(bytes=''.join([chr(int(b)) for b in yang_uuid]))


def unpack_ckpt(ckpt):
    """Unpacks a record group's checkpoint until filename.
    pid|r_id|rp_id|filename_len|filename|

    This function removes the call to "parseckpt", which was
    hardcoded into the previous query-ready.
    """
    with open(ckpt, 'rb') as infile:
        pid, r_id, rp_id, cnt = struct.unpack('<I2QI', infile.read(24))
        filename = struct.unpack('<{0}s'.format(cnt), infile.read(cnt))[0][:-1]
    return pid, r_id, rp_id, filename


def parse_ckpts():
    """Parses all checkpoints in the replay database."""
    ckpts = list()
    logs = glob.glob('/data/replay_logdb/rec_*')
    for l in logs:
        try:
            pid, r_id, _, filename = unpack_ckpt(path.join(l, 'ckpt'))
        except struct.error:
            log.warning("replay index {0} is corrupted.".format(l))
            continue
        ckpts.append((l, pid, r_id, filename))
    return ckpts


def proc_index(psql_conn):
    """Creates a process index, which creates a table in psql db that maps
    pid and exec names to their replay index.

    This function removes the call to "proc_index", which was hardcoded
    into preious query-reader.
    """
    log_path = "/data/replay_logdb"

    # Creates rec_index table table is it does not exist.
    cur = psql_conn.cursor()
    cur.execute("CREATE TABLE IF NOT EXISTS rec_index \
                (procname varchar(100),dir varchar(100));")
    cur.execute("DELETE FROM rec_index;")

    ckpts = parse_ckpts()
    for ckpt in ckpts:
        l, pid, r_id, filename = ckpt
        # Insert mapping into db. 
        cur.execute(
            "INSERT INTO rec_index (procname, dir) SELECT %s, %s \
            WHERE NOT EXISTS (SELECT 0 FROM rec_index where procname = %s);",
            (str(pid) + filename, l, str(pid) + filename))


def create_victim(record_log):
    """Creates a victim process that will eventually become the replayed
    process. Currently this will start the replay."""

    pid = os.fork()
    if pid:
        log.info("Waiting for replay setup.")
        time.sleep(5)
        # Attach pin to child.
        attach(pid)
    else:
        # Make child the victim.
        register_replay(record_log)
        #start_replay()


def register_replay(logdir, follow_splits=False, save_mmap=False):
    """Registers the calling process to be replayed."""

    class REGISTER_DATA(Structure):
        _fields_ = [
            ('pid', c_int),
            ('pin', c_int),
            ('logdir', c_char_p),
            ('linker', c_char_p),
            ('fd', c_int),
            ('follow_splits', c_int),
            ('save_mmap', c_int)
        ]

    fd = os.open('/dev/spec0', os.O_RDWR)
    if not fd:
        raise ReplayError("Failed to open /dev/spec0")


    # Send message to /dev/spec0 to register the replay.
    log.info("linker {0}".format(get_linker()))
    reg = REGISTER_DATA(pid=os.getpid(),
                        pin=0,
                        logdir=logdir,
                        linker=get_linker(),
                        fd=fd, follow_splits=follow_splits, save_mmap=1)
    REPLAY_REGISTER = IOR(ord('u'), 0x15, REGISTER_DATA)
    rc = ioctl(fd, REPLAY_REGISTER, reg)

    libc = ctypes.CDLL(None)
    syscall = libc.syscall

    #rc = syscall(59, c_char_p("sudo /bin/sh"), c_char_p("argv"), c_char_p("envp"))
    rc = os.execvp("dump", ["dump"])
    log.debug("rc value {0}".format(rc))


def start_replay():
    """Starts the actual replaying. Assumes register_replay has already
    been called succesfully"""
    argc = len(sys.argv)
    argv = (LP_c_char * (argc + 1))
    os.execvp("/bin/ls", argv, argv)

def get_linker():
    """Returns path to linker. (requires root.)"""
    with open(conf_serv['replay']['linker'], 'r') as infile:
        return '/'.join(infile.read().split())

def attach(pid, pin_tool=None):
    """Attaches pin tool to the replay system."""

    pin_tool = conf_serv['replay']['libdft']
    args = {
        '-publish_to_kafka' : 'true',
        '-kafka_server' : conf_serv['kafka']['address'],
        '-kafka_topic' : 'test',
        '-query_id' : '1234',
        '-subject_uuid' : '1234',
        '-local_principal' : '1234',
        '-tag_count' : '14'
    }
    log.info("Attaching pin to pid: {0}.".format(pid))
    cmd = ["sudo pin -pid {0} -t {1}".format(pid, pin_tool)]
    cmd_F = [cmd.extend([k,v]) for k, v in args.items()]
    log.debug(' '.join(cmd))
    p = subprocess.Popen(cmd, stderr=subprocess.PIPE, stdout=subprocess.PIPE, shell=True)
    print p.communicate()


def main():
    register_replay("/data/replay_logdb/rec_8193")

if __name__ == '__main__':
    main()

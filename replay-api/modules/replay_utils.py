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
import click

from common import *

log = logging.getLogger(__name__)

class Subject(object):
    logdir = None

    def __init__(self, **kwargs):
        #Expects pid, path, uuid, principal
        for k, v in kwargs.iteritems():
            setattr(self, k, v)


def normal_to_yang_uuid(n_uuid):
    """Converts yang's uuid to a normal uuid.
    yang - 240 9 156 0 0 0 0 0 0 0 0 0 0 0 0 32
    normal - f0099c00-0000-0000-0000-000000000020
    """
    normal = uuid.UUID(n_uuid) if type(str(n_uuid)) is str else n_uuid
    return ' '.join([str(ord(b)) for b in normal.bytes])


def yang_to_normal_uuid(yang_uuid):
    """Converts yang's uuid to a normal uuid."""
    return uuid.UUID(bytes=''.join([chr(int(b)) for b in yang_uuid.split(' ')]))


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
        ckpts.append((l, pid, r_id, os.path.basename(filename)))
    return ckpts



def get_subjects_to_taint(psql_conn):
    """ Extracts the subjects that need to be tainted during the replay."""

    # Get the subjects in the subgraph that need to be tainted.
    query = """SELECT DISTINCT subject.pid, subject.path, subject.uuid, \
            subject.local_principal from subject INNER JOIN subgraph  \
            ON  subject.uuid = subgraph.subject_uuid \
            WHERE subgraph.query_id = query_id"""
    # Find the replay logs for a subject.
    query_rec = """SELECT dir FROM rec_index WHERE procname = %s"""

    cur = psql_conn.cursor()

    # Get subjects related to this query.
    subjects = list()
    cur.execute(query)
    row = cur.fetchone()
    while row:
        s = Subject(**dict(zip(['pid', 'path', 'uuid', 'local_principal'], row)))
        subjects.append(s)
        row = cur.fetchone()

    # Get the replay logs related to this query.
    for subj in subjects:
        procname = str(subj.pid) + subj.path
        cur.execute(query_rec, (procname,))
        row = cur.fetchone()
        if row:
            subj.logdir = row[0]
    return subjects


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

        log.debug("Inserting: {0} {1} {2} {3}".format(l, pid, r_id, filename))
        cur.execute(
            "INSERT INTO rec_index (procname, dir) SELECT %s, %s \
            WHERE NOT EXISTS (SELECT 0 FROM rec_index where procname = %s);",
            (str(pid) + filename, l, str(pid) + filename))


def create_victim(subject, query):
    """Creates a victim process that will eventually become the replayed
    process. Currently this will start the replay."""

    pin_log = path.join(conf_serv['replay']['pinlog_store'],
                        'pin_replay_{0}'.format(query._id))

    # Set cmd line args for tainting.
    taint_args = {
        '-publish_to_kafka' : conf_serv['kafka']['publish'],
        '-kafka_server' : conf_serv['kafka']['address'],
        '-kafka_topic' : str(query._id),
        '-create_avro_file' : str(conf_serv['kafka']['avro']),
        '-query_id' : query._id,
        '-subject_uuid' : subject.uuid,
        '-local_principal' : subject.local_principal,
        '-logfile' : pin_log
    }

    pid = os.fork()
    if pid:
        log.info("Waiting for replay setup.")
        time.sleep(1)
        # Attach pin to child.
        attach(pid, taint_args)
    else:
        # Make child the victim.
        register_replay(subject.logdir)

    pid, status = os.waitpid(pid, 0)
    print pid, status


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
                        pin=1,
                        logdir=logdir,
                        linker=get_linker(),
                        fd=fd, follow_splits=follow_splits, save_mmap=0)
    REPLAY_REGISTER = IOR(ord('u'), 0x15, REGISTER_DATA)
    rc = ioctl(fd, REPLAY_REGISTER, reg)

    rc = os.execvp("dumb", ["dumb"])
    log.debug("rc value {0}".format(rc))


def get_linker():
    """Returns path to linker. (requires root.)"""
    with open(conf_serv['replay']['linker'], 'r') as infile:
        return '/'.join(infile.read().split())

def attach(pid, args):
    """Attaches pin tool to the replay system."""
    libdft = conf_serv['replay']['libdft']
    cmd = ['pin', '-pid', str(pid), '-t', libdft]
    [cmd.extend([k,str(v)]) for k, v in args.items()]
    print cmd
    log.info("Attaching pin to pid: {0}.".format(pid))
    log.debug(' '.join(cmd))
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    print p.communicate()


@click.group()
def cli():
    pass

@cli.command("y2n")
@click.argument("uuid", required=True)
def y2n(uuid):
    """ Convert a yang uuid to a normal uuid and print."""

    print yang_to_normal_uuid(uuid)

@cli.command("test-replay")
@click.argument("log", required=True, default="/data/replay_logdb/rec_8193")
def test_replay(log):
    pass
    #create_victim(log)

def main():
    cli()

if __name__ == '__main__':
    main()

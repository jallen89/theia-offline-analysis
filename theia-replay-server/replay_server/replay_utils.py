"""Utility functions for setting up replay."""
import sys
from fcntl import ioctl
from ioctl_opt import IOR
from os import path
import subprocess
import time
import os
import psycopg2
import struct
import glob
from ctypes import *
import ctypes
import uuid
import click

from common import *

log = logging.getLogger(__name__)

class ReplayError(Exception):
    pass

class Subject(object):
    logdir = None

    def __init__(self, **kwargs):
        #Expects pid, path, uuid, principal
        for k, v in kwargs.iteritems():
            setattr(self, k, v)

    def __str__(self):
        return "{0} {1} {2}".format(self.pid, self.path, self.uuid)

    def __eq__(self, other):
        if self.pid == other.pid and self.path == other.path:
            return True
        else:
            return False

    def __ne__(self, other):
        return not self.__eq__(other)

    def __hash__(self):
        return hash(str(self.pid) + str(self.path))


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

    This function removes the call to "parseckpt", which was
    hardcoded into the previous query-ready.
    """
    #log.debug("Opening ckpt {0}".format(ckpt))
    try:
        with open(ckpt, 'rb') as infile:
            pid, r_id, rp_id, cnt = struct.unpack('<I2QI', infile.read(24))
            filename = struct.unpack('<{0}s'.format(cnt), infile.read(cnt))[0][:-1]
    except IOError:
        err_msg = "Error: Bad Checkpoint for {0}" .format(ckpt)
        log.debug(err_msg)
        raise ReplayError(err_msg)

    return pid, r_id, rp_id, filename


def parse_ckpts():
    """Parses all checkpoints in the replay database."""
    ckpts = list()
    uuid_size = 8
    log.debug("Globbing records.")
    logs = glob.glob('/data/{0}/replay_logdb/rec_*'.format('?'*uuid_size))
    log.debug("Finished Globbing records.")
    for l in logs:
        try:
            pid, r_id, _, filename = unpack_ckpt(path.join(l, 'ckpt'))
            ckpts.append((l, pid, r_id, os.path.basename(filename)))
        except struct.error:
            log.warning("replay index {0} is corrupted.".format(l))
            continue
        except ReplayError:
            #FIXME. If this ckpt is related to the query, then it needs to 
            # be represented in the status.
            continue
    return ckpts


def get_subjects_to_taint(psql_conn):
    """ Extracts the subjects that need to be tainted during the replay."""

    # Get the subjects in the subgraph that need to be tainted.
    query = """SELECT DISTINCT subject.pid, subject.path, subject.uuid, \
subject.local_principal, \
subgraph.event_type, subgraph.event_size \
from subject INNER JOIN subgraph  \
ON  subject.uuid = subgraph.subject_uuid \
WHERE subgraph.query_id = query_id"""
    # Find the replay logs for a subject.
    query_rec = """SELECT dir FROM rec_index WHERE procname = %s"""

    cur = psql_conn.cursor()

    # Get subjects related to this query.
    subjects = list()
    log.debug("executing query\n{0}".format(query))
    cur.execute(query)
    log.debug("Finished executing query\n{0}".format(query))
    row = cur.fetchone()
    while row:
        s = Subject(**dict(zip(
            ['pid', 'path', 'uuid', 'local_principal', 'event_type', 'event_size'],
            row))
        )
        subjects.append(s)
        row = cur.fetchone()

    # Get the replay logs related to this query.
    print set(subjects), subjects
    for subj in set(subjects):
        procname = str(subj.pid) + subj.path
        cur.execute(query_rec, (procname,))
        row = cur.fetchone()
        if row:
            subj.logdir = row[0]
    log.debug("----Found subjects: {0}".format('\n'.join([s.__str__() for s in subjects])))
    return subjects


def proc_index(psql_conn):
    """Creates a process index, which creates a table in psql db that maps
    pid and exec names to their replay index.

    This function removes the call to "proc_index", which was hardcoded
    into preious query-reader.
    """

    log_path = conf_serv['replay']['replay_logdb']
    # Creates rec_index table table is it does not exist.
    cur = psql_conn.cursor()
    cur.execute("CREATE TABLE IF NOT EXISTS rec_index \
                (procname varchar(100),dir varchar(100));")
    cur.execute("DELETE FROM rec_index;")

    ckpts = parse_ckpts()
    for ckpt in ckpts:
        l, pid, r_id, filename = ckpt
        # Insert mapping into db. 

        #log.debug("Inserting into rec_index: {0} {1} {2} {3}".format(
        #    l, pid, r_id, filename))

        cur.execute(
            "INSERT INTO rec_index (procname, dir) SELECT %s, %s \
            WHERE NOT EXISTS (SELECT 0 FROM rec_index where procname = %s);",
            (str(pid) + filename, l, str(pid) + filename))

    # Commit transaction.
    psql_conn.commit()


def create_victim(subject, query):
    """Creates a victim process that will eventually become the replayed
    process. Currently this will start the replay."""

    pin_log = path.join(conf_serv['replay']['pinlog_store'],
                        'pin_replay_{0}'.format(query._id))

    # Set cmd line args for tainting.
    log.debug("Begin tainting pid=({0}) subject_name=({1}) logdir=({2})".format(
        subject.pid, subject.path, subject.logdir))

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

    event_size = subject.event_size;
    event_type = subject.event_type;

    pid = os.fork()
    if pid:
        log.info("Waiting for replay setup.")
        time.sleep(1)
        # Attach pin to child.
        attach(pid, taint_args, event_size, event_type)
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
                        fd=fd,
                        follow_splits=follow_splits,
                        save_mmap=0)

    REPLAY_REGISTER = IOR(ord('u'), 0x15, REGISTER_DATA)
    rc = ioctl(fd, REPLAY_REGISTER, reg)

    rc = os.execvp("dumb", ["dumb"])
    log.debug("rc value {0}".format(rc))


def get_linker():
    """Returns path to linker. (requires root.)"""
    with open(conf_serv['replay']['linker'], 'r') as infile:
        return '/'.join(infile.read().split())

def attach(pid, args, event_size, event_type):
    """Attaches pin tool to the replay system."""

    if event_type in ['EVENT_READ','EVENT_RECV']:
        if event_size in range(1, 2^8):
            libdft = conf_serv['replay']['libdft-u8']
        elif event_size in range(2^8, 2^16):
            libdft = conf_serv['replay']['libdft-u16']
        elif event_size in range(2^16, 2^32):
            libdft = conf_serv['replay']['libdft-u32']
        elif event_size in range(2^32, 2^64):
            libdft = conf_serv['replay']['libdft-u64']
        else:
            libdft = conf_serv['replay']['libdft-u64']
    else:
        libdft = conf_serv['replay']['libdft-u64']
    pin_home = conf_serv['replay']['pin_home']
    cmd = [pin_home, '-pid', str(pid), '-t', libdft]
    [cmd.extend([k,str(v)]) for k, v in args.items()]
    print cmd
    log.info("Attaching pin to pid: {0}.".format(pid))
    log.debug(' '.join(cmd))
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    print p.communicate()


@click.group()
def cli():
    pass

@cli.command("psql-subjects")
def show_files():
    c = psycopg2.connect(**dict(conf_serv.items('psql')))
    cur = c.cursor()
    cur.execute("SELECT * FROM file")

    row = cur.fetchone()
    while row:
        print row
        row = cur.fetchone()


@cli.command("psql-show")
@click.argument("obj-type")
def show_files(obj_type):
    """
    List objects in table

    obj-type - (subject, file, netflow, query, rec_index)
    """
    c = psycopg2.connect(**dict(conf_serv.items('psql')))
    cur = c.cursor()
    print str(obj_type)
    cur.execute("""SELECT * FROM {0};""".format(obj_type))

    row = cur.fetchone()
    while row:
        if obj_type in ['subject', 'file', 'netflow']:
            print row, yang_to_normal_uuid(row[0])
        else:
            print row
        row = cur.fetchone()

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

@cli.command("uuid-split")
@click.argument("host-obj", required=True)
def uuid_split(host_obj):
    """Return host and object uuid.
       @host-obj -- host-object uuid.

       ex: "128 55 12 110 82 84 0 240 8 96 0 0 0 0 0 112-1 0 208 15 40 9 24 0 0 0 0 0 209 153 215 91"
    """
    host, obj = host_obj.split('-')
    print yang_to_normal_uuid(obj), yang_to_normal_uuid(host)

@cli.command("test-ckpts")
def test_ckpts():
    print parse_ckpts()

def main():
    cli()

if __name__ == '__main__':
    main()

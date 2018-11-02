"""Manages the play queue for TA teams."""
import logging
import json
import sys
import time
from multiprocessing import Process

from redis import Redis
from redis import client
client.StrictRedis.hincrbyfloat = client.StrictRedis.hincrby
from rq import Queue, Connection, Worker, timeouts

from replay_server import Query, DBManager, Analysis, search

#from replay_server.search import *

from cdm import *
import replay_utils as replay
from common import *


log = logging.getLogger(__name__)


def handle_query(query):
    """ Handles starting the reachability analysis, replay, and tainting."""
    log.debug("Handling query: {0}".format(query))
    DBManager().update_status(query._id, "Reachability Analysis")

    analysis = Analysis()
    try:
        analysis.reachability(query)
    except timeouts.JobTimeoutException:
        DBManager().update_status(query._id, 'timeout: reachability analysis')
        return

    #if not use_replay:
    #    return

    # Update status to tainting.
    try:
        use_replay = conf_serv.getboolean('debug', 'replay')
        # Initializes the replay.
        DBManager().update_status(query._id, "Preparing Replay")
        subjects = analysis.prepare_replay()
        DBManager().update_status(query._id, "Replaying")
        for subject in subjects:
            if subject.logdir:
                # Replay and tainting begin.
                replay.create_victim(subject, query)

        #Create the list of tag nodes here. (cdm.py)
        records = search.get_overlay(analysis.psql_db.cursor(), query._id)
        #If only 1 record, then create list. [record]
        analysis.publish_overlay(query, records)
    
    except timeouts.JobTimeoutException:
       DBManager().update_status(query._id, 'timeout: taint analysis')
       return 

    DBManager().update_status(query._id, "Finished.")

    # Close connections.
    #analysis.neo_db.close()
    analysis.psql_db.close()
    analysis.publisher.shutdown()


class QueueManager(object):
    """A class for managing the replay queue."""

    redis = Redis(**dict(conf_serv.items('redis')))
    q = Queue(connection=redis)
    def __init__(self):
        job = None
        log.debug("Initializing QueueManager.")
        with Connection(connection=self.redis):
            Process(target=Worker(self.q).work).start()


    def new_query(self, query):
        """Adds new query to the working queue."""
        log.debug("{0} added to work queue.".format(query))
        job = self.q.enqueue(handle_query, query,
                             timeout="{0}m".format(query.timeout))

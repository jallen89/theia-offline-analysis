"""Manages the play queue for TA teams."""
import logging
import json
import sys

from redis import Redis
from rq import Queue

from modules import Query, DBManager, Analysis
import replay_utils as replay
from common import *

log = logging.getLogger(__name__)


def handle_query(query):
    """ Handles starting the reachability analysis, replay, and tainting."""
    log.debug("Handling query: {0}".format(query))
    DBManager().update_status(query._id, "Reachability Analysis")

    analysis = Analysis()
    analysis.reachability(query)
    analysis.prepare_replay()
    # Update status to tainting.
    DBManager().update_status(query._id, "Replaying")
    # Initializes the replay.
    subjects = analysis.prepare_replay()
    for subject in subjects:
        if subject.logdir:
            # Replay and tainting begin.
            replay.create_victim(subject.logdir)




class QueueManager(object):
    """A class for managing the replay queue."""

    q = Queue(connection=Redis())
    def __init__(self):
        log.debug("Initializing QueueManager.")

    def new_query(self, query):
        """Adds new query to the working queue."""
        log.debug("{0} added to work queue.".format(query))
        job = self.q.enqueue(handle_query, query)

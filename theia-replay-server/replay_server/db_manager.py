"""Manages MongoDB that holds received queries."""
import sys
import logging
import pymongo

from common import *
from . import Query

log = logging.getLogger(__name__)

class DBManager(object):
    """Manages the database that holds the queries sent over
    by a TA2 team."""

    m_addr = conf_serv['mongo']['address']
    m_conn = pymongo.MongoClient(m_addr)

    def insert(self, q):
        """Insert a query into database."""
        log.debug("Inserting {0} into DB.".format(q))
        self.m_conn.db.query.insert_one(q.dump())

    def update_status(self, q_id, status):
        """Update the status parameter of a query.

        status options: (inqueue, running, finished.)
        """
        q = self.query(q_id)
        q.status = status
        self.m_conn.db.query.save(q.dump())

    def query(self, q_id):
        """Queries the db for a received query."""
        q = self.m_conn.db.query.find_one(q_id)
        if q:
            return Query.from_json(q)
        else:
            log.warning("Query ID {0} not in DB.".format(q_id))
            return None

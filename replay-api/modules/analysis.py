"""A Wrapper around the reachability analysis."""
import uuid

from neo4jrestclient.client import GraphDatabase
import psycopg2

from common import *
from query import Query
from search import *
import replay_utils as replay

log = logging.getLogger(__name__)

class Analysis(object):
    """Wraps reachability analysis for setup and intialization."""

    # Connect to neo4j and psql.
    neo4j = dict(conf_serv.items('neo4j'))
    neo_db = GraphDatabase(**neo4j)
    # Connect to psql.
    psql = dict(conf_serv.items('psql'))
    psql_db = psycopg2.connect(**psql)

    def analysis(self, query):
        """Call search.py"""

    def forward_analysis(self, query):
        """Applies forward analysis on the respective query."""
        #uuid = normal_to_yang_uuid(query.uuid)
        uuid = str(query.uuid)
        paths = forward_query(self.neo_db, uuid, None, 2, query.end)
        #TODO: Call code to insert into DB.
        return

    def backward_analysis(self, query):
        """Applies backward analysis on the respective query."""
        uuid = replay.normal_to_yang_uuid(str(query.uuid))
        paths = backward_query(self.neo_db, uuid, None, 2, query.start)
        print len(paths)
        #TODO: Call code to insert into DB.
        return

    def point2point(self, query):
        """Applies point2point anlaysis on the respective query."""
        uuid1 = normal_to_yang_uuid(query.uuid)
        #TODO: Call code to insert into DB.
        return

    def prepare_replay(self):
        """Setups the environment for the replay."""
        # Creates the process index mappings.
        replay.proc_index(self.psql_db)


if __name__ == '__main__':
    q = Query('theia-1', 'backward',
              "f0099c00-0000-0000-0000-000000000020",
              00000000, 99999999999)

    Analysis().backward_analysis(q)
    print q




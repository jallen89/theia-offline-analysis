"""A Wrapper around the reachability analysis."""
import uuid

from neo4jrestclient.client import GraphDatabase
import psycopg2

from common import *
from query import Query
from search import *
import replay_utils as replay

log = logging.getLogger(__name__)


class AnalysisError(Exception):
    """ Error class for Analysis."""
    pass


class Analysis(object):
    """Wraps reachability analysis for setup and intialization."""
    # handlers to call based on which query type was requested.
    que_handlers = {
        BACKWARD : backward_query,
        FORWARD : forward_query,
    }

    # Connect to neo4j and psql.
    neo_db = GraphDatabase(**dict(conf_serv.items('neo4j')))
    psql_db = psycopg2.connect(**dict(conf_serv.items('psql')))

    def reachability(self, query):
        """Call search.py"""
        self.query = query
        # Complete reachability Analysis.
        uuid = replay.normal_to_yang_uuid(str(query.uuid))
        paths = self.que_handlers[query.query_type](self.neo_db, uuid, None, 2, query.start)
        self._insert_subgraph(paths)
        # Setup the Replay environment.

    def prepare_replay(self):
        """Setups the environment for the replay."""
        # Creates the process index mappings.
        replay.proc_index(self.psql_db)
        subjects = replay.get_subjects_to_taint(self.psql_db)
        return subjects

    def _insert_subgraph(self, paths):
        """Create subgraph table in psql."""
        insert_paths(self.neo_db, self.psql_db, self.query, paths) 


def test_backward():
    """ Tests the backward analysis, assume the uuid is in the
    neo4j database."""
    uuid = "f0099c00-0000-0000-0000-000000000020",
    q = Query('theia-1', 'backward', 00000000, 99999999999)
    Analysis().analysis(q)


if __name__ == '__main__':
    pass

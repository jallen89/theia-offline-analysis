"""A Wrapper around the reachability analysis."""
import uuid

from neo4jrestclient.client import GraphDatabase
import psycopg2

from common import *
from query import Query
from search import *
from publisher import TheiaPublisher
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
        POINT2POINT : point2point_query
    }

    def __init__(self):
        # Connect to neo4j and psql.
        self.neo_db = GraphDatabase(**dict(conf_serv.items('neo4j')))
        self.psql_db = psycopg2.connect(**dict(conf_serv.items('psql')))
        self.publisher = TheiaPublisher(True, True)

    def reachability(self, query):
        """Call search.py"""
        self.query = query
        # Complete reachability Analysis.
        log.debug("Applying reachability analysis.")
        uuid = replay.normal_to_yang_uuid(str(query.uuid))
        host_uuid = replay.normal_to_yang_uuid(str(query.host_uuid))
        if query.uuid_end:
            uuid_end = replay.normal_to_yang_uuid(str(query.uuid_end))
        else:
            uuid_end = None

        # Get the paths for the query.
        paths = self.que_handlers[query.query_type](
            self.neo_db, host_uuid, uuid, uuid_end, query.hops,
            query.start, query.end)

        log.debug("Inserting subgraph.")
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

    def publish_overlay(self, query, records):
        """ Publishes @records to kafka server with the
        topic name set to the unique value @query._id

            @query - The Query object we are replaying.
            @records a LIST of records to publish.
        """
        self.publisher.set_topic(str(query._id))
        self.publisher.publish(records)


def test_backward():
    """ Tests the backward analysis, assume the uuid is in the
    neo4j database."""
    uuid = "f0099c00-0000-0000-0000-000000000020",
    q = Query('theia-1', 'backward', 00000000, 99999999999)
    Analysis().analysis(q)


if __name__ == '__main__':
    pass

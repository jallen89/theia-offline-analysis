#!/usr/bin/python
import sys
import argparse
import getpass
import time
from collections import Counter

from neo4jrestclient.client import GraphDatabase
import psycopg2

from common import *
from cdm import *
from replay_utils import *

log = logging.getLogger(__name__)

# Inserting DB entries if they don't already exist
# https://stackoverflow.com/questions/4069718/postgres-insert-if-does-not-exist-already

def get_overlay(psql_cursor, qid):
    query = "SELECT (uuid,origin_uuids,qid,type,tag_uuid,subject_uuid) FROM tag_overlay where qid='{0}'".format(qid)
    psql_cursor.execute(query)   
    rows = psql_cursor.fetchall()
  
    records = []
    for r in rows:
        #publish one by one
        records.add(create_prov_tag_node(r[5],r[4],r[0],r[1],r[2],r[3],"0"))

    return records;


# Inserts node into relational database
def insert_node(psql_cursor,neo4j_db,r, qid):
    if r[1] == 'SUBJECT':
        # Get filepath of this subject
        q = "MATCH (n1:NODE {{uuid:'{0}', nodeType:'SUBJECT'}})-[n2:NODE {{nodeType:'EVENT', type:'EVENT_EXECUTE'}}]->(n3:NODE {{nodeType:'FILE'}}) return n3.name,n1.local_principal".format(r[0])
        results = neo4j_db.query(q)
        local_principal = "";
        for r2 in results:
            exe_pos = r2[0].rfind('/')
            local_principal = r2[1]
            if exe_pos == -1:
                exe_name = r2[0];
            else:
                exe_name = r2[0][exe_pos+1:]
            psql_cursor.execute("INSERT INTO subject (uuid,path,pid,local_principal) SELECT '{0}','{1}',{2},'{3}' WHERE NOT EXISTS (SELECT uuid FROM subject WHERE uuid='{0}')".format(r[0],exe_name,r[2],local_principal))

        # If no results were returned, still insert the subject
        if len(results) == 0:
            psql_cursor.execute("INSERT INTO subject (uuid,path,pid,local_principal) SELECT '{0}','',{1},'' WHERE NOT EXISTS (SELECT uuid FROM subject WHERE uuid='{0}')".format(r[0],r[2]))

    elif r[1] == 'FILE':
        psql_cursor.execute("INSERT INTO file (uuid,path,query_id) SELECT '{0}','{1}','{2}' WHERE NOT EXISTS (SELECT uuid FROM file WHERE uuid='{0}' AND query_id='{2}')".format(r[0],r[3],qid))

    elif r[1] == 'NETFLOW':
        psql_cursor.execute("INSERT INTO netflow (uuid,local_ip,local_port,remote_ip,remote_port, query_id)) SELECT '{0}','{1}',{2},'{3}',{4},'{5}' WHERE NOT EXISTS (SELECT uuid FROM file WHERE uuid='{0}' AND query_id='{5}')".format(r[0],r[4],r[5],r[6],r[7],qid))

# Performs forward query
def forward_query(db, uuid1, uuid2, depth, end_timestamp):
    q = None
    if uuid2 == None:
        q = "MATCH (n:NODE {{uuid: '{0}'}}) ".format(uuid1, depth)
    else:
        q = "MATCH (n:NODE {{uuid: '{0}'}}) MATCH (m:NODE {{uuid: '{1}'}}) ".format(uuid1, uuid2)

    q += "MATCH path=(n)-[*..{0}]->(m) ".format(depth)
    q += "WITH RELATIONSHIPS(path) AS rels, range(1, length(path)-1) AS idx, path "
    # Each node must have a timestamp later than the last node
    q += "WHERE ALL(i IN idx WHERE (toInteger(rels[i].ts) >= toInteger(rels[i-1].ts)) AND (toInteger(rels[i].ts) <= {0})) ".format(end_timestamp)
    q += "RETURN path"

    results = db.query(q)
    return results

# Performs backward query
def backward_query(db, uuid1, uuid2, depth, start_timestamp):
    q = None
    if uuid2 == None:
        q = "MATCH (n:NODE {{uuid: '{0}'}}) ".format(uuid1, depth)
    else:
        q = "MATCH (n:NODE {{uuid: '{0}'}}) MATCH (m:NODE {{uuid: '{1}'}}) ".format(uuid1, uuid2)

    q += "MATCH path=(n)<-[*..{0}]-(m) ".format(depth)
    q += "WITH RELATIONSHIPS(path) AS rels, range(1, length(path)-1) AS idx, path "
    # Each node must have a timestamp earlier than the last node
    q += "WHERE ALL(i IN idx WHERE (toInteger(rels[i].ts) <= toInteger(rels[i-1].ts)) AND (toInteger(rels[i].ts) >= {0})) ".format(start_timestamp)
    q += "RETURN path"

    results = db.query(q)
    return results

# This is a union of both forward_query() and backward_query()
def point2point_query(db, uuids, depth, start_timestamp, end_timestamp):
    #XXX. Fix me.
    uuid1 = uuids.split('-')[0]
    uuid2 = uuids.split('-')[1]

    # Perform forward query
    forward_paths = forward_query(db, uuid1, uuid2, depth, end_timestamp)

    # Perform backward query
    backward_paths = backward_query(db, uuid1, uuid2, depth, start_timestamp)

    # Return combined paths
    return list(forward_paths) + list(backward_paths)

# https://gist.github.com/namnv609/f462c194e80ed4048cd2
class Password(argparse.Action):
    def __call__(self, parser, namespace, values, option_string):
        if values is None:
            values = getpass.getpass(prompt=option_string+': ')

        setattr(namespace, self.dest, values)

def main():

    # Get arguments
#    parser = argparse.ArgumentParser()
#
#    parser.add_argument('--neo4j-username', dest ='neo4j_username',
#                        help='Neo4j Username', required=True)
#    parser.add_argument('--psql-username', dest ='psql_username',
#                        help='PostgreSQL Username', required=True)
#
#    parser.add_argument('--neo4j-password', action=Password, nargs='?',
#                        dest ='neo4j_password', help='Neo4j Password', required=True)
#    parser.add_argument('--psql-password', action=Password, nargs='?',
#                        dest ='psql_password', help='PostgreSQL Password', required=True)
#    parser.add_argument('--neo4j-db', dest ='neo4j_db', help='Neo4j', required=True)
#
#    parser.add_argument('--psql-db', dest ='psql_database', help='PostgreSQL Database',
#                        required=True)
#
#    parser.add_argument('--depth', dest ='depth', help='Depth to Query', type=int,
#                        required=True)
#
#    parser.add_argument('--query-id', dest ='query_id', help='Query UUID',
#                        required=True)
#    parser.add_argument('--start-timestamp', dest ='start_timestamp',
#                        help='Start Timestamp', required=True)
#    parser.add_argument('--end-timestamp', dest ='end_timestamp',
#                        help='End Timestamp', required=True)
#
#    group = parser.add_mutually_exclusive_group(required=True)
#    group.add_argument('--forward', dest='forward', help='Forward Query')
#    group.add_argument('--backward', dest='backward', help='Backward Query')
#    group.add_argument('--point2point', dest='point', help='Point-to-point Query')
#
#    args = parser.parse_args()
#
    # Connect to Neo4j server
    neo4j_db = GraphDatabase(args.neo4j_db, username=args.neo4j_username,
                             password=args.neo4j_password)
    print "neo4j connected"
    # Connect to PostgreSQL database
    psql_db = psycopg2.connect(database=args.psql_database, user=args.psql_username,
                               password=args.psql_password, host="127.0.0.1", port="5432")



def node_exists(neo4j_db, self, uuid):
    """ Verify a node exists."""
    q = "MATCH (n:NODE {{uuid:'{0}'}}) RETURN n".format(uuid)
    results = neo4j_db.query(q)
    return True if len(results) else False


def get_edge_meta(neo4j_db, eID):
    """ Return metadata related to edge."""
    q = 'MATCH ()-[n]->() WHERE ID(n)={0} RETURN n.uuid, n.type, n.ts, n.size'.format(eID)
    results = neo4j_db.query(q)[0]
    log.debug("edge meta {0}".format(results))
    return results


def get_node_meta(neo4j_db, nID):
    """Return metadata related to node."""
    q = 'MATCH (n) WHERE ID(n)={0} RETURN n.uuid, n.nodeType, n.cid, n.filename, n.local_address, n.local_port, n.remote_address, n.remote_port'.format(nID)
    results = neo4j_db.query(q)
    return results

def get_paths(neo4j_db, query):
    """ Extract queries from neo4jdb. """
    start = time.time()
    if query.query_type == 'forward':
        paths = forward_query(neo4j_db, query.uuid, None, query.depth, query.end)
    elif query.query_type == 'backward':
        paths = backward_query(neo4j_db, query.uuid, None, query.depth, query.stard)
    elif query.query_type =='point2point':
        #XXX. Fix UUID arguments.
        #paths = point2point_query(neo4j_db, args.point, args.depth,
        #                          args.start_timestamp, args.end_timestamp)
        pass
    log.info('Time to query Neo4j (seconds): {0}'.format(time.time() - start))

def search(psql_db, neo4j_db, query):
    """Manages, a forward, backward, or point-to-point query."""

    # Print out stats about graph
    q = "MATCH (n) RETURN count(*)"
    result = neo4j_db.query(q)
    for r in result:
        print 'Graph\'s number of nodes: {0}'.format(r)
    q = "MATCH ()-[r]->() RETURN count(*)"
    result = neo4j_db.query(q)
    for r in result:
        print 'Graph\'s number of edges: {0}'.format(r)
    print ''

    paths = None

    # Data to insert into relational database
    query_type = None
    uuid1 = None
    uuid2 = None

    # Verify The node exists.
    if not node_exists(neo4j_db, query.uuid):
        log.error("No node with UUID {0}".format(query.uuid))
        return False
    elif (query.query_type == 'point2point' and
          not node_exists(neo4j_db, query.uuid2)):
        log.error("No node with UUID {0}".format(query.uuid2))
        return False

    # Start timer for getting paths using Neo4j
    paths = get_paths(neo4j_db, query)

    # Insert nodes and edges.
    nodetypes = Counter()
    edgetypes = Counter()
    totalnodes = set()
    totaledges = set()

    # Get information about nodes/edges involved in returned path(s)
    if len(paths) == 0:
        print 'No paths found'
        sys.exit(0)

    # Create cursor
    psql_cursor = psql_db.cursor()

    # Get query ID and create new query entry
    #qid = None
    #psql_cursor.execute("SELECT MAX(id) FROM QUERY")
    #rows = psql_cursor.fetchall()
    #for r in rows:
        # If this is the first query ever, qid is 0
        #if r[0] == None:
        #    qid = 0
        # Else, calculate the next qid
        #else:
        #    qid = int(r[0]) + 1

    # Start timer for getting path metadata
    start = time.time()

    # Enumerate through all the paths.

def insert_paths(neo4j_db, psql_db, query, paths):
    nodetypes = Counter()
    edgetypes = Counter()
    totalnodes = set()
    totaledges = set()

    psql_cursor = psql_db.cursor()

    qid = query._id
    for e, p in enumerate(paths):
        # Print status
        sys.stdout.write('Parsing Path: {0} / {1}\r'.format(e+1,len(paths)))
        sys.stdout.flush()

        #TODO - why does it always return a list of size 1?
        p = p[0]

        nodes = p['nodes']
        edges = p['relationships']
        dir = p['directions']

        # Traverse path and insert into database
        for i in range(len(nodes)-1):
            # Print visual representation of path
            # print nodes[i], dir[i], edges[i], dir[i], nodes[i+1]

            # Get node IDs and edge ID
            n1ID = nodes[i].split('/')[-1]
            n2ID = nodes[i+1].split('/')[-1]
            eID = edges[i].split('/')[-1]

            # Variables to fill in to insert into relational database
            subject_uuid = None
            file_uuid = None
            netflow_uuid = None
            event_uuid = None
            results = None
            event_uuid, event_type, event_ts, event_size = get_edge_meta(neo4j_db, eID)

            # Only interested in certain edges
            if event_type not in ['EVENT_READ','EVENT_WRITE','EVENT_SEND','EVENT_RECV']:
                continue

            # Add for statistics
            if eID not in totaledges:
                edgetypes[event_type] += 1

            results = get_node_meta(neo4j_db, n1ID)
            # Get each node's metadata
            for r in results:
                # Fill in variables for relational database
                log.debug("{0}".format(r))
                if r[1] == 'SUBJECT':
                    subject_uuid = r[0]
                elif r[1] == 'FILE':
                    file_uuid = r[0]
                elif r[1] == 'NETFLOW':
                    netflow_uuid = r[0]

                # Insert node into relational database
                insert_node(psql_cursor, neo4j_db, r, qid)

                # Add for statistics
                if n1ID not in totalnodes:
                    nodetypes[r[1]] += 1

            q = 'MATCH (n) WHERE ID(n)={0} RETURN n.uuid, n.nodeType, n.cid, n.filename, \
            n.local_address, n.local_port, n.remote_address, n.remote_port'.format(n2ID)
            results = neo4j_db.query(q)
            for r in results:
                log.debug("results: {0}{1}".format( r[0], r[1]))
                # Fill in variables for relational database
                if r[1] == 'SUBJECT':
                    subject_uuid = r[0]
                elif r[1] == 'FILE':
                    file_uuid = r[0]
                elif r[1] == 'NETFLOW':
                    netflow_uuid = r[0]

                # Insert node into relational database
                insert_node(psql_cursor,neo4j_db,r, qid)

                # Add for statistics
                if n2ID not in totalnodes:
                    nodetypes[r[1]] += 1

            # Insert path (subgraph) into database
            log.debug("{0}---{1}{2}-----{3}".format(subject_uuid, event_uuid, event_type, file_uuid))
            psql_cursor.execute(
                "INSERT INTO subgraph (query_id,subject_uuid,event_type,event_uuid, \
                event_size,event_ts,file_uuid,netflow_uuid) VALUES ('{0}','{1}','{2}', \
                '{3}',{4},'{5}','{6}','{7}')".format(
                    qid,subject_uuid,event_type,event_uuid,event_size,event_ts,file_uuid,
                    netflow_uuid))

            # For statistics to print out below...
            totalnodes.add(n1ID)
            totalnodes.add(n2ID)
            totaledges.add(eID)

    # Make pretty line ending
    sys.stdout.write('\n')
    sys.stdout.flush()
    print ''
    uuid2 = None
    uuid1 = normal_to_yang_uuid(query.uuid)
    # Create query entry
    psql_cursor.execute("INSERT INTO query (id,type,uuid1,uuid2) SELECT '{0}','{1}','{2}', \
                        '{3}' WHERE NOT EXISTS (SELECT id FROM query WHERE id='{0}')".format(
                            qid,query.query_type,uuid1,uuid2))

    # Commit to database
    psql_db.commit()

if __name__ == '__main__':
    main()

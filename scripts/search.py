#!/usr/bin/python

import sys
import argparse
import getpass
import time
from collections import Counter

from neo4jrestclient.client import GraphDatabase
import psycopg2

# Inserting DB entries if they don't already exist
# https://stackoverflow.com/questions/4069718/postgres-insert-if-does-not-exist-already

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
            psql_cursor.execute("INSERT INTO subject (uuid,path,pidi,local_principal) SELECT '{0}','',{1},'' WHERE NOT EXISTS (SELECT uuid FROM subject WHERE uuid='{0}')".format(r[0],r[2]))

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
    parser = argparse.ArgumentParser()

    parser.add_argument('--neo4j-username', dest ='neo4j_username', help='Neo4j Username', required=True)
    parser.add_argument('--psql-username', dest ='psql_username', help='PostgreSQL Username', required=True)

    parser.add_argument('--neo4j-password', action=Password, nargs='?', dest ='neo4j_password', help='Neo4j Password', required=True)
    parser.add_argument('--psql-password', action=Password, nargs='?', dest ='psql_password', help='PostgreSQL Password', required=True)

    parser.add_argument('--psql-db', dest ='psql_database', help='PostgreSQL Database', required=True)

    parser.add_argument('--depth', dest ='depth', help='Depth to Query', type=int, required=True)

    parser.add_argument('--query-id', dest ='query_id', help='Query UUID', required=True)

    parser.add_argument('--start-timestamp', dest ='start_timestamp', help='Start Timestamp', required=True)
    parser.add_argument('--end-timestamp', dest ='end_timestamp', help='End Timestamp', required=True)

    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--forward', dest='forward', help='Forward Query')
    group.add_argument('--backward', dest='backward', help='Backward Query')
    group.add_argument('--point2point', dest='point', help='Point-to-point Query')

    args = parser.parse_args()

    # Connect to Neo4j server
    neo4j_db = GraphDatabase('http://143.215.129.19:7474', username=args.neo4j_username, password=args.neo4j_password)

    # Connect to PostgreSQL database
    psql_db = psycopg2.connect(database=args.psql_database, user=args.psql_username, password=args.psql_password, host="127.0.0.1", port="5432")

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

    # Check to see if nodes exist
    if args.forward:
        q = "MATCH (n:NODE {{uuid:'{0}'}}) RETURN n".format(args.forward)
        results = neo4j_db.query(q)
        if len(results) == 0:
            print 'Error. UUID: {0} does not exist'.format(args.forward)
            sys.exit(0)
    elif args.backward:
        q = "MATCH (n:NODE {{uuid:'{0}'}}) RETURN n".format(args.backward)
        results = neo4j_db.query(q)
        if len(results) == 0:
            print 'Error. UUID: {0} does not exist'.format(args.backward)
            sys.exit(0)
    elif args.point:
        q = "MATCH (n:NODE {{uuid:'{0}'}}) RETURN n".format(args.point.split('-')[0])
        results = neo4j_db.query(q)
        if len(results) == 0:
            print 'Error. UUID: {0} does not exist'.format(args.point.split('-')[0])
            sys.exit(0)
        q = "MATCH (n:NODE {{uuid:'{0}'}}) RETURN n".format(args.point.split('-')[1])
        results = neo4j_db.query(q)
        if len(results) == 0:
            print 'Error. UUID: {0} does not exist'.format(args.point.split('-')[1])
            sys.exit(0)

    # Start timer for getting paths using Neo4j
    start = time.time()

    # Call appropriate function
    if args.forward:
        paths = forward_query(neo4j_db, args.forward, None, args.depth, args.end_timestamp)
        query_type = 'FORWARD'
    elif args.backward:
        paths = backward_query(neo4j_db, args.backward, None, args.depth, args.start_timestamp)
        uuid1 = args.backward
        query_type = 'BACKWARD'
    elif args.point:
        paths = point2point_query(neo4j_db, args.point, args.depth, args.start_timestamp, args.end_timestamp)
        uuid1 = args.point.split('-')[0]
        uuid2 = args.point.split('-')[1]
        query_type = 'POINT2POINT'

    print 'Time to query Neo4j (seconds): {0}'.format(time.time()-start)
    print ''

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
    qid = args.query_id

    # Start timer for getting path metadata
    start = time.time()

    # For each returned path
    for e,p in enumerate(paths):
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
            event_type = None
            event_ts = None
            event_size = None

            # Get edge's metadata
            q = 'MATCH ()-[n]->() WHERE ID(n)={0} RETURN n.uuid, n.type, n.ts, n.size'.format(eID)
            results = neo4j_db.query(q)
            for r in results:
                # Fill in variables for relational database
                event_uuid = r[0]
                event_type = r[1]
                event_ts = r[2]
                event_size = r[3]

            # Only interested in certain edges
            if event_type not in ['EVENT_READ','EVENT_WRITE','EVENT_SEND','EVENT_RECV']:
                continue

            # Add for statistics
            if eID not in totaledges:
                edgetypes[event_type] += 1

            # Get each node's metadata
            q = 'MATCH (n) WHERE ID(n)={0} RETURN n.uuid, n.nodeType, n.cid, n.filename, n.local_address, n.local_port, n.remote_address, n.remote_port'.format(n1ID)
            results = neo4j_db.query(q)
            for r in results:
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
                if n1ID not in totalnodes:
                    nodetypes[r[1]] += 1

            q = 'MATCH (n) WHERE ID(n)={0} RETURN n.uuid, n.nodeType, n.cid, n.filename, n.local_address, n.local_port, n.remote_address, n.remote_port'.format(n2ID)
            results = neo4j_db.query(q)
            for r in results:
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
            psql_cursor.execute("INSERT INTO subgraph (query_id,subject_uuid,event_type,event_uuid,event_size,event_ts,file_uuid,netflow_uuid) VALUES ('{0}','{1}','{2}','{3}',{4},'{5}','{6}','{7}')".format(qid,subject_uuid,event_type,event_uuid,event_size,event_ts,file_uuid,netflow_uuid))

            # For statistics to print out below...
            totalnodes.add(n1ID)
            totalnodes.add(n2ID)
            totaledges.add(eID)

    # Make pretty line ending
    sys.stdout.write('\n')
    sys.stdout.flush()
    print ''

    # Create query entry
    psql_cursor.execute("INSERT INTO query (id,type,uuid1,uuid2) SELECT '{0}','{1}','{2}','{3}' WHERE NOT EXISTS (SELECT id FROM query WHERE id='{0}')".format(qid,query_type,uuid1,uuid2))

    # Commit to database
    psql_db.commit()

    print 'Time to parse paths (seconds): {0}'.format(time.time()-start)
    print ''

    # Print stats about number and types of nodes/edges
    print 'Subgraph information:'
    print '====================='
    print 'Total unique nodes: {0}'.format(len(totalnodes))
    print 'Total unique edges: {0}'.format(len(totaledges))
    print 'Types of nodes:'
    for t,c in nodetypes.most_common():
        print '    {0}: {1}'.format(t,c)
    print 'Types of edges:'
    for t,c in edgetypes.most_common():
        print '    {0}: {1}'.format(t,c)

    # Close connection
    psql_db.close()

main()

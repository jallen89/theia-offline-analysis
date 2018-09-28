#!/usr/bin/python

"""A client-side utility to interact with Theia's replay system."""
import os
import pickle
import requests

import click
from marshmallow import pprint, Schema, fields
import configparser

from modules import Query
from modules.common import *

CLIENT_OUT = '.{0}-theia-client-state.pkl'

config = configparser.ConfigParser()
config.read("client.cfg")


class Client(object):
    """Client class to save persistent info."""

    def __init__(self):
        self.team_id = config['metadata']['team']
        self.outfile = CLIENT_OUT.format(self.team_id)
        self.q_id = 0
        self.export_client()

    @classmethod
    def load_client(cls):
        """Loads a saved client from CLIENT_OUT"""
        team_id = config['metadata']['team']
        c_out = CLIENT_OUT.format(team_id)

        # Create new client.
        if not os.path.exists(c_out):
            return Client()

        with open(c_out, 'r') as infile:
            client = pickle.load(infile)

        return client

    def export_client(self):
        """Save client to CLIENT_OUT."""
        with open(self.outfile, 'w') as out:
            pickle.dump(self, out)

    def q_inc(self):
        """Gets q_id and increments."""
        self.q_id += 1
        return self.q_id - 1


@click.group()
def cli():
    pass


@cli.command("forward-query")
@click.argument("uuid", required=True)
@click.argument("start-ts", required=True)
@click.argument("end-ts", required=True)
@click.argument("hops", required=True)
def forward_query(uuid, start_ts, end_ts, hops):
    """Sends a request to make a forward query to Theia's replay system.

    Arguments:
    uuid - The UUID of the CDM record in which the query will be applied.

    start - The start timestamp. Event records with timestamps prior to
    the start timestamp will not be involved in the query.

    end - The end timestamp. Event records with timestamps after the end
    timestamp will not be involved in the query.

    hops - The analysis will span across 'hops' nodes from the target
    node.

    Returns:
        Returns a receipt for this query along with the query's ID.
    """
    team_id = config['metadata']['team']
    query = Query(team_id + str(client.q_inc()),
                  FORWARD, uuid, start_ts, end_ts, hops)
    r_addr = config["metadata"]["replay_server_ip"]
    query.send(r_addr)
    client.export_client()


@cli.command("backward-query")
@click.argument("uuid", required=True)
@click.argument("start-ts", required=True)
@click.argument("end-ts", required=True)
@click.argument("hops", required=True)
def backward_query(uuid, start_ts, end_ts, hops):
    """Sends a request to make a backward query to Theia's replay system.

    Arguments:
    uuid - The UUID of the CDM record in which the query will be applied.

    start - The start timestamp. Event records with timestamps prior to
    the start timestamp will not be involved in the query.

    end - The end timestamp. Event records with timestamps after the end
    timestamp will not be involved in the query.

    hops - The analysis will span across 'hops' nodes from the target
    node.

    Returns:
        Returns a receipt for this query along with the query's ID.
    """
    team_id = config['metadata']['team']
    query = Query(team_id + str(client.q_inc()),
                  BACKWARD, uuid, start_ts, end_ts, hops)
    r_addr = config["metadata"]["replay_server_ip"]
    query.send(r_addr)
    client.export_client()


@cli.command("point-to-point-query")
@click.argument("uuid-begin", required=True)
@click.argument("uuid-end", required=True)
@click.argument("start-ts", required=True)
@click.argument("end-ts", required=True)
@click.argument("hops", required=True)
def point_to_point_query(uuid_begin, uuid_end, start_ts, end_ts, hops):
    """Sends a request to make a point-to-point query to Theia's replay system.

    Arguments:
    uuid_start - The UUID of the CDM record in which the query will start.

    uuid_end - The UUID of the CDM record in which the query will end.

    start - The start timestamp. Event records with timestamps prior to
    the start timestamp will not be involved in the query.

    end - The end timestamp. Event records with timestamps after the end
    timestamp will not be involved in the query.

    hops - The analysis will span across 'hops' nodes from the target
    node.

    Returns:
        Returns a receipt for this query along with the query's ID.
    """
    # XXX. Add optional paramter for uuid2, but make it required for
    # point-to-point analysis.
    team_id = config['metadata']['team']
    query = Query(team_id + str(client.q_inc()), POINT2POINT, uuid_begin,
                  start_ts, end_ts, uuid_end, hops)
    r_addr = config["metadata"]["replay_server_ip"]
    query.send(r_addr)
    client.export_client()


@cli.command("status")
@click.argument("query-id")
def status(query_id):
    """Sends a request for a status update for a query that has already been
    requested.

    Arguments:

        query-id - The query-id, which is the query id returned by the query
        command. (ex: ripe-1)

    Returns:
        A json object with metadata related to the query's status.
    """
    r_addr = config["metadata"]["replay_server_ip"]
    r = requests.get("http://{0}/query/{1}".format(r_addr, query_id))
    print "Query Status: {0}".format(r.content)


if __name__ == '__main__':
    client = Client.load_client()
    cli()

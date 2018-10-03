#!/usr/bin/python2.7

"""Restful API for Theia's replay system."""
import logging
import sys

from flask import Flask, request
import pymongo
from flask_restful import Api, Resource
from marshmallow import Schema, fields, pprint
import configparser

from replay_server import QueueManager, DBManager, Query
from replay_server.common import *

log = logging.getLogger(__name__)
logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)


app = Flask(__name__)
api = Api(app)


class TheiaQuery(Resource):
    """Interface for Theia's Replay Query."""

    def __init__(self):
        m_addr = conf_serv['mongo']['address']
        self.query_que = QueueManager()
        self.db_manager = DBManager()

    def get(self, queryID):
        """Get the status of a query."""
        q = self.db_manager.query(queryID)
        if q:
            return q.status
        else:
            return "Query not found", 404

    def post(self, queryID):
        """Creates a new query object. """
        q = Query.from_json(request.form)
        log.debug("Inserting query: {0}".format(q))
        q.status = "inqueue"
        self.db_manager.insert(q)
        self.query_que.new_query(q)

    def delete(self, queryID):
        """Cancel a query ."""
        # TODO: Add capability to cancel a query.
        return "{} is canceled.".format(queryID), 200

    def _update_status(q, status):
        """Updates query status."""
        q.status = status
        self.db_manager.update_status(q, status)


api.add_resource(TheiaQuery, "/query/<string:queryID>", endpoint="query")

if __name__ == '__main__':
    host = conf_serv['flask']['host']
    port = conf_serv['flask']['port']
    debug = conf_serv['flask']['debug']

    app.run(host=host, port=port, debug=debug)

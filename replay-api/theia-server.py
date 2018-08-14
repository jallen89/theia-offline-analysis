"""Restful API for Theia's replay system."""
import logging
import sys

from flask import Flask, request
import pymongo
from flask_restful import Api, Resource
from marshmallow import Schema, fields, pprint
import configparser

from modules import QueueManager, DBManager, Query

log = logging.getLogger(__name__)
logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)

config = configparser.ConfigParser()
config.read("server.cfg")

app = Flask(__name__)
api = Api(app)


class TheiaQuery(Resource):
    """Interface for Theia's Replay Query."""

    def __init__(self):
        m_addr = config['mongo']['address']
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
app.run(debug=True)

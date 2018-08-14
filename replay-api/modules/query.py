"""Query class used for sending and receiving query requests."""
import logging
import requests
import datetime as dt
import sys

import mongoengine as me
from marshmallow_mongoengine import ModelSchema
from marshmallow import fields, pprint, post_load
import configparser

#TODO Remove, this is in server code.
config = configparser.ConfigParser()
config.read("client.cfg")

log = logging.getLogger(__name__)
logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)


class Query(me.Document):
    """MongoDB Model for a Query object to send to Theia's replay server."""

    _id = me.StringField(primary_key=True)
    query_type = me.StringField()
    uuid = me.StringField()
    start = me.StringField()
    end = me.StringField()
    status = me.StringField(default=None)

    @classmethod
    def from_json(cls, query_json):
        """Creates a query object from a json object."""
        return QuerySchema().load(query_json).data

    def pprint(self):
        """Prints query in json format. """
        pprint(self.query_schema.dump(self).data)

    def send(self):
        """Send query to replay server."""
        data = QuerySchema().dump(self).data
        log.debug("Sending: {0}".format(data))
        #TODO make replay_server_ip a input parameter.
        r_addr = config["metadata"]["replay_server_ip"]
        requests.post("http://{0}/query/{1}".format(r_addr, self._id), data=data)

    def dump(self):
        return QuerySchema().dump(self).data

    def __str__(self):
        return '<Query({0}>'.format(self.dump())


class QuerySchema(ModelSchema):
    """Schema for serializing and deserializing queries."""
    class Meta:
        model = Query

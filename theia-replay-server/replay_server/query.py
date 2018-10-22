"""Query class used for sending and receiving query requests."""
import logging
import requests
import datetime as dt
import sys

import mongoengine as me
from marshmallow_mongoengine import ModelSchema
from marshmallow import fields, pprint, post_load, schema
import configparser


log = logging.getLogger(__name__)
logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)


class Query(me.Document):
    """MongoDB Model for a Query object to send to Theia's replay server."""

    _id = me.StringField(primary_key=True)
    query_type = me.StringField()
    uuid = me.StringField()
    host_uuid = me.StringField()
    start = me.StringField()
    end = me.StringField()
    hops = me.StringField()
    uuid_end = me.StringField(default=None)
    status = me.StringField(default="Initialized.")

    @classmethod
    def from_json(cls, query_json):
        """Creates a query object from a json object."""
        q = QuerySchema().load(query_json)
        print "from_json: ", type(q), q
        return q.data

    def pprint(self):
        """Prints query in json format. """
        pprint(self.query_schema.dump(self))

    def send(self, address):
        """Send query to replay server."""
        data = QuerySchema().dump(self).data
        log.debug("Sending: {0}".format(data))
        requests.post("http://{0}/query/{1}".format(address, self._id),
                       data=data)
    def dump(self):
        q = QuerySchema().dump(self)
        print "dump type: ", type(q)
        if type(q) == schema.MarshalResult:
            return q.data
        else:
            return q

    def __str__(self):
        return '<Query({0}>'.format(self.dump())


class QuerySchema(ModelSchema):
    """Schema for serializing and deserializing queries."""
    class Meta:
        model = Query

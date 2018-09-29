from pkg_resources import Requirement, resource_filename
import configparser
import sys
import logging
import os

# Configuration for server.
server_config = resource_filename(Requirement.parse("theia_replay_server"), "server.cfg")
print server_config, os.path.exists(server_config)
conf_serv = configparser.ConfigParser()
conf_serv.read(server_config)

logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)

# Query types.
BACKWARD = "backward"
FORWARD = "forward"
POINT2POINT = "point2point"

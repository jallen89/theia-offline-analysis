import configparser
import sys
import logging
import os

# Configuration for server.
if os.path.exists("server.cfg"):
    conf_serv = configparser.ConfigParser()
    conf_serv.read("server.cfg")

logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)

# Query types.
BACKWARD = "backward"
FORWARD = "forward"
POINT2POINT = "point2point"

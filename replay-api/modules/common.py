import configparser
import sys
import logging

# Configuration for server.
conf_serv = configparser.ConfigParser()
conf_serv.read("server.cfg")

logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)

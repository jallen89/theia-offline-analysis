from pkg_resources import Requirement, resource_filename
import configparser
import sys
import logging
import os

logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)
log = logging.getLogger(__name__)

# Configuration for server.
CONFIG_PATH =  "/etc/conf/theia/server.cfg"
if os.path.exists(CONFIG_PATH):
    conf_serv = configparser.ConfigParser()
    conf_serv.read(CONFIG_PATH)
else:
    log.error("{0} does not exist!.".format(CONFIG_FILE))
    sys.exit()




# Query types.
BACKWARD = "backward"
FORWARD = "forward"
POINT2POINT = "point2point"

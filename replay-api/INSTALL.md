### Ubuntu 12.04 Deps

Build
===

```
pip install -r requirements.txt
```


Dependencies
===

1. Requires MongoDB 2.6 (https://futurestud.io/tutorials/how-to-install-mongodb-2-6-on-ubuntudebian)
```
https://futurestud.io/tutorials/how-to-install-mongodb-2-6-on-ubuntudebian
echo 'deb http://downloads-distro.mongodb.org/repo/ubuntu-upstart dist 10gen' | sudo tee /etc/apt/sources.list.d/mongodb.list  
sudo apt-get update  
sudo apt-get install mongodb-org  
sudo apt-get install mongodb-org=2.6.2 mongodb-org-server=2.6.2 mongodb-org-shell=2.6.2 mongodb-org-mongos=2.6.2 mongodb-org-tools=2.6.2  
```



Config files
===

There are two config files, `client.cfg` and `server.cfg`, which is expected to be in the top 
level directory (this can be updated if needed).

theia-server
===

The `theia-server.py` is used to start the restful API for the server side. 


theia-client
===
The `theia-client.py` can be used to send queries to the theia-replay server. 

```shell
Usage: theia-client.py backward-query [OPTIONS] UUID START_TS END_TS HOPS

  Sends a request to make a backward query to Theia's replay system.

  Arguments: uuid - The UUID of the CDM record in which the query will be
  applied.

  start - The start timestamp. Event records with timestamps prior to the
  start timestamp will not be involved in the query.

  end - The end timestamp. Event records with timestamps after the end
  timestamp will not be involved in the query.

  hops - The analysis will span across 'hops' nodes from the target node.

  Returns:     Returns a receipt for this query along with the query's ID.

Options:
  --help  Show this message and exit.
```


Troubleshooting (Ubuntu 12.04 setup)
===

1. Outdated Redis server version for Ubuntu 12.04. The `rq` python module uses the `HINCRBYFLOAT` command, which is only 
available on >Redis 2.6.0. However, the current package distributed on Ubuntu 12.04 is `2.2.12`. A hack around this was to modify
`/usr/local/lib/python2.7/dist-packages/redis/client.py:1977` to change HINCRBYFLOAT to HINCRBY. Version 2.6.0 can be found at
[redis-2.6.0](https://github.com/antirez/redis/tree/2.6).

```ResponseError: Command # 4 (HINCRBYFLOAT rq:worker:target-1.28676 total_working_time 7668) of pipeline caused error: 
Command # 4 (HINCRBYFLOAT rq:worker:target-1.28676 total_working_time 7668) of pipeline caused error: unknown command 'HINCRBYFLOAT'
```

2. The mongo_engine python module expects a MongoDB version >2.6.4, which is newer than the version distributed to Ubuntu 12.04. 
This was addressed using the following:

`MongoDB >2.6.4`:

```shell
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv 7F0CEB10  
echo 'deb http://downloads-distro.mongodb.org/repo/ubuntu-upstart dist 10gen' | sudo tee /etc/apt/sources.list.d/mongodb.list  
sudo apt-get update  
sudo apt-get install mongodb-org  
```

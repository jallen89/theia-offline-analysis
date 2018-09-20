Theia DIFT Module
================


Overview
========
* DIFT Client Interface
* DIFT REST Interface




DIFT Client Interface
=====================

### Usage

To get a list of commands.

```
$ ./theia-client --help
Usage: theia-client [OPTIONS] COMMAND [ARGS]...

Options:
  --help  Show this message and exit.

Commands:
  backward-query        Sends a request to make a backward query to...
  forward-query         Sends a request to make a forward query to...
  point-to-point-query  Sends a request to make a point-to-point...
  status                Sends a request for a status update for a...
```

To get information related to a specific command:

```
$ ./theia-client backward-query --help
Usage: theia-client backward-query [OPTIONS] UUID START_TS END_TS HOPS

  Sends a request to make a backward query to Theia's replay system.

  Arguments: 

  uuid -  The UUID of the CDM record in which the query will be applied.

  start - The start timestamp. Event records with timestamps prior to the
          start timestamp will not be involved in the query.

  end -   The end timestamp. Event records with timestamps after the end
          timestamp will not be involved in the query.

  hops -  The analysis will span across 'hops' nodes from the target node.
  
  Returns: 
      Returns a receipt for this query along with the query's ID.

Options:
  --help  Show this message and exit.
```


DIFT REST Interface
====================




EndPoints
---

`query` 

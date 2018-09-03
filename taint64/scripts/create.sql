DROP TABLE rec_index;
DROP TABLE tag_mapping;
DROP TABLE subgraph;
DROP TABLE subject;
DROP TABLE file;
DROP TABLE netflow;
DROP TABLE query;

CREATE TABLE rec_index(
    procname VARCHAR(4096),                                                                
    dir VARCHAR(4096)
);


CREATE TABLE tag_mapping(
    query_id VARCHAR(64),
    subject_uuid VARCHAR(64),
    global_tag VARCHAR(64),
    local_tag VARCHAR(64)
);

CREATE TABLE subgraph(
    query_id VARCHAR(64),
    subject_uuid VARCHAR(64),
    event_type VARCHAR(16),
    event_uuid VARCHAR(64),
    event_size INT,
    event_ts VARCHAR(20),
    file_uuid VARCHAR(64),
    netflow_uuid VARCHAR(64)
);

CREATE TABLE subject(
    uuid VARCHAR(64) UNIQUE NOT NULL PRIMARY KEY,
    path VARCHAR(4096),
    pid INT,
    local_principal VARCHAR(64)
);

CREATE TABLE file(
    uuid VARCHAR(64) NOT NULL,
    path VARCHAR(4096) NOT NULL,
    query_id VARCHAR(64),
    primary key (uuid, query_id)
);

CREATE TABLE netflow(
    uuid VARCHAR(64) NOT NULL,
    local_ip VARCHAR(15),
    local_port INT,
    remote_ip VARCHAR(15),
    remote_port INT,
    query_id VARCHAR(64) NOT NULL,
    primary key (uuid, query_id)
);

CREATE TABLE query(
    id VARCHAR(64) UNIQUE NOT NULL PRIMARY KEY,
    type VARCHAR(11),
    uuid1 VARCHAR(64),
    uuid2 VARCHAR(64)
);

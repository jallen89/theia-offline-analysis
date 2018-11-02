DROP_OVERLAY="DROP TABLE tag_overlay;"

CREATE_OVERLAY=\
"""
CREATE TABLE tag_overlay(
uuid VARCHAR(64) UNIQUE NOT NULL PRIMARY KEY,
type VARCHAR(11),
offset_t INT,
origin_uuids VARCHAR(1024),
qid VARCHAR(64),
tag_uuid VARCHAR(64),
subject_uuid VARCHAR(64));
"""

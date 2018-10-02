"""Builds CDM Records of a specific type."""

from replay_utils import *
from search import *
from tc.schema.records.record_generator import Util
from tc.schema.records.record_generator import CDMProvenanceTagNodeGenerator as ProvGen

SOURCE = "SOURCE_LINUX_SYSCALL_THEIA"
CDM_VERSION = '19'

def y2n_and_set(uuid):
    print uuid
    u = yang_to_normal_uuid(uuid).int
    return Util._set_by_value(u, 16)



def create_prov_tag_node(subject_uuid, tag_uuid, object_uuid,
                         source_tag_uuid_set, sc, host_uuid):

    record = dict()
    provenance_tag_node = dict()

    # Convert UUIDs.
    subject_uuid = y2n_and_set(subject_uuid)
    tag_uuid = y2n_and_set(tag_uuid)
    object_uuid = y2n_and_set(object_uuid)
    host_uuid = y2n_and_set(host_uuid)
    # Create Provenance Tag Record.
    provenance_tag_node['tagId'] = tag_uuid
    provenance_tag_node['flowObject'] = object_uuid
    provenance_tag_node['subject'] = subject_uuid
    provenance_tag_node['systemCall'] = sc
    provenance_tag_node['opcode'] = "TAG_OP_UNION"
    provenance_tag_node["properties"] = dict()

    # Add source tag nodes.
    tag_uuids = set()
    for t_uuid in source_tag_uuid_set.split('|'):
        tag_uuids.add(yang_to_normal_uuid(t_uuid).bytes)
    provenance_tag_node['tagIds'] = list(tag_uuids)

    # Create TCCDMDatum Record to wrap the node.
    rec_type = "RECORD_PROVENANCE_TAG_NODE"
    return create_wrapper(provenance_tag_node, rec_type, tag_uuid)

def create_wrapper(datum, rec_type, host_uuid):
    record = {}
    # Create Record to wrap around node.
    record["datum"] = datum
    record["source"] = SOURCE
    record["CDMVersion"] = CDM_VERSION
    record["hostId"] = host_uuid
    record["sessionNumber"] = -1
    record["type"] = rec_type
    return record

def gen_prov_record():
    u = '112 9 141 1 0 0 0 0 0 0 0 0 0 0 0 32'
    return create_prov_tag_node(u, u, u, u, "test", u)


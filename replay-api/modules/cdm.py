"""Builds CDM Records of a specific type."""

from replay_utils import *
from search import *

SOURCE = "SOURCE_LINUX_SYSCALL_THEIA"
CDM_VERSION = 19

def create_prov_tag_node(subject_uuid, tag_uuid, object_uuid,
                         source_tag_uuid_set, sc, host_uuid):

    record = dict()
    provenance_tag_node = dict()

    # Convert UUIDs.
    tag_uuid = yang_to_normal_uuid(tag_uuid)
    object_uuid = yang_to_normal_uuid(object_uuid)
    subject_uuid = yang_to_normal_uuid(subject_uuid)

    # Create Provenance Tag Record.
    provenance_tag_node['tagId'] = tag_uuid
    provenance_tag_node['flowObject'] = object_uuid
    provenance_tag_node['subject'] = subject_uuid
    provenance_tag_node['systemCall'] = sc
    provenance_tag_node['TagOpCode'] = "TAG_OP_UNION"


    # Add source tag nodes.
    tag_uuids = set()
    for t_uuid in source_tag_uuid_set:
        tag_uuids.add(yang_to_normal_uuid(t_uuid))
    provenance_tag_node['tagIds'] = tag_uuids

    # Create TCCDMDatum Record to wrap the node.
    return create_wrapper(provenance_tag_node, rec_type, host_uuid)


def create_wrapper(datum, rec_type, host_uuid):

    # Create Record to wrap around node.
    record["datum"] = datum
    record["source"] = SOURCE
    record["CDMVersion"] = CDM_VERSION
    record["hostId"] = host_uuid
    #XXX. We don't support sessionNumber in replay queries. This can be
    # extracted from original CDM.
    record["sessionNumber"] = -1
    record["type"] = rec_type

    return record


"""Publishes CDM Records to Kafka Server and file."""
from confluent_kafka import avro
from confluent_kafka import Producer
from tc.services import kafka
from tc.schema.serialization import AvroGenericSerializer, Utils
from tc.schema.serialization.kafka import KafkaAvroGenericSerializer
from tc.schema.records.record_generator import RecordGeneratorFactory
from replay_server import cdm

from common import *
log = logging.getLogger(__name__)


class PublisherError(Exception):
    """Error class for TheiaPublisher."""
    pass


class TheiaPublisher(object):
    """Publishes CDM Records to Kafka Server and file."""

    def __init__(self, publish_kafka, publish_file, topic=None):
        """Create a CDM Producer

        :param publish_kafka: If True publish data to Kafka server.
        :param publish_file: If True publish data to file.
        :param topic:  Set the topic name. If None, the topic name
        will be the value of the query id. It is preferred to set
        to leave this value to None.
        """

        self.topic = topic
        self.conf_kafka = conf_serv['kafka']
        self.publish_kafka = publish_kafka
        self.publish_file = publish_file

        # Init schema
        schema_file = self.conf_kafka['schema']
        self.schema = Utils.load_schema(schema_file)

        # Init and Register publishers.
        self.publishers = list()
        if publish_kafka:
            self._init_kafka()
            self.publishers.append(self._publish_kafka)

        if publish_file:
            self._init_file()
            self.publishers.append(self._publish_file)

        self._init_stats()


    def set_topic(self, new_topic):
        """Set the Kafka topic name.

        :param new_topic: The new topic name.
        """
        self.topic = new_topic

    def publish(self, records):
        """Produce records to all registered publishers.

        :param record: A list of records to publish.
        """
        if type(records) is not list:
            log.debug("Found only 1 record")
            records = [records]

        # Publish Record to publishers registered in init.
        log.debug("{0} records to publish.".format(len(records)))
        for record in records:
            for publisher in self.publishers:
                publisher(record)

        if self.publish_kafka:
            self.producer.flush()


    def shutdown(self):
        """Shutdown Publisher by flushing all records to Server and
        closing output file."""

        if self.publish_kafka:
            self.producer.flush()

        if self.publish_file:
            self.f_serializer.close_file_serializer()

    def print_stats(self):
        """ Print stats related to records published."""
        log.info("{0} records published to kafka.".format(self.k_records))
        log.info("{0} records published to file.".format(self.f_records))


    def _init_stats(self):
        """Init stats."""
        self.k_records = 0
        self.f_records = 0

    def _init_kafka(self):
        """Initialize Kafka Server based on config.kafka."""

        # Configure Security Protocol.
        k_conf = conf_serv["kafka"]
        if conf_serv["use_ssl"]:
            config["security.protocol"] = "ssl"
            config["ssl.ca.location"] = k_conf["ca_location"]
            config["ssl.certificate.location"] = k_conf["cert_location"]
            config["ssl.key.location"] = k_conf["key_location"]
            config["ssl.key.password"] = k_conf["key_password"]
        else:
            config["security.protocol"] = "plaintext"

        config["bootstrap.servers"] = self.conf_kafka["address"]

        # Build Producer
        log.info("Starting Producer.")
        self.producer = Producer(config)
        self.k_serializer = KafkaAvroGenericSerializer(self.schema, True)

    def _init_file(self):
        """Initializes a file serializer."""
        out = self.conf_kafka['overlay_out']
        self.f_serializer = AvroGenericSerializer(self.schema, out)

    def _publish_kafka(self, record):
        """Publishes a list of records to Kafka Server.\n"""
        message = self.k_serializer.serialize(self.topic, record)
        self.producer.produce(self.topic, value=message, key="kafka-key")
        self.k_records += 1
        self.producer.poll(0)

    def _publish_file(self, record):
        """Publishes a record to output file."""
        print "serializing record!"
        self.f_serializer.serialize_to_file(record)
        self.f_records += 1

    def shutdown(self):
        if self.publish_file:
            self.f_serializer.close_file_serializer()
        if self.publish_kafka:
            self.producer.flush()




def gen_records(how_many, serializer):
    """ Generate @how_many random records for testing."""
    records = list()
    for i in range(how_many):
        rec = RecordGeneratorFactory.get_record_generator(serializer)
        records.append(rec.generate_random_record(i))
    return records

def test_kafka_publisher(topic, how_many):
    """Sends @how_many random records to Kafka Server."""
    publisher = TheiaPublisher(True, False, "test-python")
    records = gen_records(how_many, publisher.k_serializer)
    publisher.publish(records)
    publisher.shutdown()
    publisher.print_stats()

def test_file_publisher(how_many):
    """Writes @how_many random records to file."""
    publisher = TheiaPublisher(False, True, None)
    records = gen_records(how_many, publisher.f_serializer)

    publisher.publish(records)
    publisher.shutdown()
    publisher.print_stats()

def test_prov_publish():
    rec = cdm.gen_prov_record()
    print rec
    publisher = TheiaPublisher(True, True, "prov-test")
    publisher.publish([rec])
    publisher.shutdown()
    publisher.print_stats()

if __name__ == '__main__':
    test_prov_publish()
    test_kafka_publisher("python-test-50", 50)
    test_kafka_publisher("python-test-50", 1)
    test_file_publisher(50)
    test_file_publisher(100)

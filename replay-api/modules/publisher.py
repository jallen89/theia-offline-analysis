
from confluent_kafka import avro
from confluent_kafka import Producer
from tc.services import kafka
from tc.schema.serialization import AvroGenericSerializer, Utils
from tc.schema.serialization.kafka import KafkaAvroGenericSerializer
from tc.schema.records.record_generator import RecordGeneratorFactory

from common import *
log = logging.getLogger(__name__)

class PublisherError(Exception):
    """Error class for TheiaPublisher."""
    pass

class TheiaPublisher(object):

    def publish(self, records):
        """Produce records to all registered publishers."""

        for record in records:
            for publisher in self.publishers:
                publisher(record)

        self.producer.flush()

    def __init__(self, publish_kafka, publish_file, topic):
        self.topic = topic
        self.conf_kafka = conf_serv['kafka']
        self.publish_kafka = publish_kafka
        self.publish_file = publish_file

        # Init and Register publishers.
        self.publishers = list()
        if publish_kafka:
            self._init_kafka()
            self.publishers.append(self._publish_kafka)

        if publish_file:
            #self._init_file(self):
            self.publishers.append(self._publish_file)

        # Init schema
        schema_file = self.conf_kafka['schema']
        schema = Utils.load_schema(schema_file)
        self.serializer = KafkaAvroGenericSerializer(schema, True)


    def _init_kafka(self):
        # Create config dict. for kafka producer.

        config =  {
            'bootstrap.servers' : self.conf_kafka['address']
        }

        log.info("Starting Producer.")
        self.producer = Producer(config)


    def _init_file(self):
        """Initializes a file serializer."""
        pass


    def _publish_kafka(self, record):
        """Publishes a list of records to Kafka Server.\n"""

        log.debug("Publishing record {0} to Kafka.".format(record))
        message = self.serializer.serialize(self.topic, record)
        self.producer.produce(self.topic, value=message, key="kafka-key")
        self.producer.poll(0)


    def _publish_file(self, record):
        pass

def test_kafka_publisher(topic, how_many):
    """Sends @how_many random records to Kafka Server."""
    publisher = TheiaPublisher(True, False, "test-python")
    rec = RecordGeneratorFactory.get_record_generator(publisher.serializer)
    edge = rec.generate_random_record(how_many)
    publisher.publish([edge])

if __name__ == '__main__':
    test_kafka_publisher("python-test-50", 50)


TARGET_DIR=build
REDIS_PATH=/usr/lib/python2.7/site-packages/redis/client.py

make all: patch-redis


# Apply patch to redis, so it works with redis distributed on 12.04.
patch-redis: install
	sudo sed -i "s/HINCRBYFLOAT/HINCRBY/" $(REDIS_PATH)

install:
	sudo -s pip install $(PWD) --process-dependency-links

#XXX. The build key requires a authorized key for BBN.
build:
	pip install $(PWD) -t $(TARGET_DIR) --process-dependency-links

pep8:
	python setup.py pep8 --max-line-length=80 --show-source 


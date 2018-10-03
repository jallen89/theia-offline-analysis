from setuptools import setup, find_packages
import sys
import os

setup(
    name='theia-replay-server',
    version='0.1',
    description='Theia replay server',
    author='Joey Allen',
    author_email='jallen309@gatech.edu',
    packages=find_packages(),
    scripts=['scripts/theia-server', 'scripts/theia-client'],
    zip_safe=False,
    data_files=[('/etc/conf/theia', ['configs/server.cfg', 'configs/client.cfg']),
                ('/etc/gunicorn.d', ['configs/replay_server.gunicorn.conf',]),
                ],
    include_package_data=True,

    install_requires=[
        'tc_bbn_py>=19.20180907.0',
        'marshmallow_mongoengine>=0.9.0',
        'neo4jrestclient>=2.1.0',
        'rq>=0.12.0',
        'redis>=2.10.0',
        'psycopg2>=2.7.5',
        'Flask>=1.0.2',
        'marshmallow>=2.15.0',
        'ioctl_opt>=1.2.0',
        'click>=6.0',
        'requests>=2.19.0',
        'pymongo>=2.7.1',
        'Flask_RESTful>=0.3.6',
        'mongoengine>=0.9.0',
        'confluent_kafka>=0.11.0',
    ],

    dependency_links=[
        'git+ssh://git@git.tc.bbn.com/bbn/ta3-api-bindings-python.git@develop#egg=tc_bbn_py-19.20180907.0',
    ]
)

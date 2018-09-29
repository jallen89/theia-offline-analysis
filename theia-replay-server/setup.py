from setuptools import setup, find_packages

setup(
    name='theia-replay-server',
    version='0.1',
    description='Theia replay server',
    author='Joey Allen',
    author_email='jallen309@gatech.edu',
    packages=find_packages(),
    scripts=['scripts/theia-server.py'],
    data_files=[('config', 'configs/server.cfg')],
    zip_safe=False
)

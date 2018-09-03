# Disclaimer
* Fix paths based on where you are building the tool
# Building
* Run the following commands to setup dependencies
	* sudo apt-get install autogen autoconf libtool
	* sudo apt-get install build-essential
	* sudo apt-get install libelf-dev
	* sudo apt-get install synaptic
		* Open synaptic packet manager
		* Search for libelf
		* Mark libelfg0-dev for installation
		* Apply
  * sudo apt-get install libpqxx-dev                                             
  * Install boost 1.54
  * Install libdwarf                                                           
	  * tar -xzf libdwarf-20130207.tar.gz
	  * cd dwarf-20130207
	  * ./configure --enable-shared
	  * make
	  * sudo cp libdwarf/libdwarf.so /usr/lib/
  * Prepare pin
    * tar -xzf pin-2.13-61206-gcc.4.4.7-linux.tar.gz
    * mv pin-2.13-61206-gcc.4.4.7-linux pin-2.13
   cp pin/compiler_version_check2.H pin-2.13/source/include/pin/compiler_version_check2.H
* Build taint64
  * cd to taint64
  * Set PIN_ROOT in Makefile.config to root directory of pin
	  * For example PIN_ROOT=/home/theia/theia-ki-offline-analysis/taint64/pin-2.13
		  * Must use version 2.14 or lower. 3.X does not work.
  * SET OMNYPLAY_DIR in Makefile.config to root of theia-es folder (double check with Yang if this is needed as this might be part of the replay code and he knows about it)
    * For example OMNIPLAY_DIR = /home/theia/theia-es
* Set export PIN_HOME to root of pin
	* For example export PIN_HOME=/home/theia/theia-ki-offline-analysis/taint64/pin-2.13
* Run the following commands to build
	* rm -r build
	* autoreconf -fvi
	* mkdir build
	* cd build
	* ../configure
	* make CC=gcc-5.4 CXX=g++-5.4
# Notes
* There might be some hardcoded variables and paths we would like to fix

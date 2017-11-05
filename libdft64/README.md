# Disclaimer
* Fix paths accordingly
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
	* tar -xzf libdwarf-20130207.tar.gz
	* cd dwarf-20130207
	* ./configure --enable-shared
	* make
	* sudo cp libdwarf/libdwarf.so /usr/lib/
	* cd ..
* Go to libdft64
* Set PIN_ROOT in Makefile.config to root directory of pin
	* For example PIN_ROOT=/home/yang/Mattia/libdft64/pin
		* Must use version 2.14 or lower. 3.X does not work.
* Set export PIN_HOME to root of pin
	* For example PIN_HOME=/home/yang/Mattia/libdft64/pin
* Run the following commands to build
	* rm -r build
	* autoreconf -fvi
	* mkdir build
	* cd build
	* ../configure
	* make
* Run sudo ldconfig
# Run example
* The example reads from one file (a.txt) and write the content to another file (b.txt) it also writes a constant string to a third file (c.txt). The expected result is that the bytes in b.txt should be tainted with labels 1,2,4,8,16 while bytes in c.txt should not be tainted (e.g, label is 0).
* cd examples/1_test
* /home/yang/Mattia/libdft64/pin/intel64/bin/pinbin -injection child -follow_execv -t /home/yang/Mattia/libdft64/libdft64/build/tools/.libs/libdft-dta64.so -- ./1_test


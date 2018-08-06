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
	* For example PIN_ROOT=/home/theia/theia-es/libdft64/pin
		* Must use version 2.14 or lower. 3.X does not work.
* Set export PIN_HOME to root of pin
	* For example export PIN_HOME=/home/theia/theia-es/libdft64/pin
* Comment lines 101,102,103,116,117,118 from pin-2.13/source/include/pin/compiler_version_check2.H
* Run the following commands to build
	* rm -r build
	* autoreconf -fvi
	* mkdir build
	* cd build
	* ../configure
	* make CC=gcc-5.4 CXX=g++-5.4
* Run sudo ldconfig
# Run example
* The example reads from one file (a.txt) and write the content to another file (b.txt) it also writes a constant string to a third file (c.txt). The expected result is that the bytes in b.txt should be tainted with labels 1,2,4,8,16 while bytes in c.txt should not be tainted (e.g, label is 0).
* cd examples/1_test
* /home/theia/theia-es/libdft64/pin-2.13/intel64/bin/pinbin -injection child -follow_execv -t /home/theia/theia-es/libdft64/libdft64/build/tools/.libs/libdft-dta64.so -- ./1_test

# Run example using gdb
* In terminal 1
	* /home/mattia/PhD/Research/THEIA/Repositories/theia-es/libdft64/pin/intel64/bin/pinbin -pause_tool 60 -injection child -follow_execv -t /home/mattia/PhD/Research/THEIA/Repositories/theia-es/libdft64/libdft64/build/tools/.libs/libdft-dta64.so -- ./1_test
* In terminal 2
	* sudo gdb /home/mattia/PhD/Research/THEIA/Repositories/theia-es/libdft64/pin/intel64/bin/pinbin 20196
	* add-symbol-file /home/mattia/PhD/Research/THEIA/Repositories/theia-es/libdft64/libdft64/build/tools/.libs/libdft-dta64.so 0x7fbf70d5f2c0 -s .data 0x7fbf712b0540 -s .bss 0x7fbf712c24c0
	* b main
	* c

# Install taint
* Install postgress library for Theia_Tagging
	* sudo apt-get install libpqxx-dev
* Install boost 1.54
	* sudo apt-get install python-dev
	* sudo apt-get install libbz2-dev
	* tar -xvfs

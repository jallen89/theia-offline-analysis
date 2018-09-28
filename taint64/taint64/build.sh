export PIN_HOME=$PWD/../../taint64/pin-2.13;
rm -r build;
autoreconf -fvi;

mkdir -p build/u8;
cp src/Makefile.am.u8 src/Makefile.am
cp tools/Makefile.am.u8 tools/Makefile.am
cd build/u8;
../../configure;
make CC=gcc-5.4 CXX=g++-5.4;
cp tools/.libs/libdft-dta64.so ../libdft-dta64-u8.so
cd ../..;

mkdir -p build/u16;
cp src/Makefile.am.u16 src/Makefile.am
cp tools/Makefile.am.u16 tools/Makefile.am
cd build/u16;
../../configure;
make CC=gcc-5.4 CXX=g++-5.4;
cp tools/.libs/libdft-dta64.so ../libdft-dta64-u16.so
cd ../..;

mkdir -p build/u32;
cp src/Makefile.am.u32 src/Makefile.am
cp tools/Makefile.am.u32 tools/Makefile.am
cd build/u32;
../../configure;
make CC=gcc-5.4 CXX=g++-5.4;
cp tools/.libs/libdft-dta64.so ../libdft-dta64-u32.so
cd ../..;

mkdir -p build/u64;
cp src/Makefile.am.u64 src/Makefile.am
cp tools/Makefile.am.u64 tools/Makefile.am
cd build/u64;
../../configure;
make CC=gcc-5.4 CXX=g++-5.4;
cp tools/.libs/libdft-dta64.so ../libdft-dta64-u64.so
cd ../..;



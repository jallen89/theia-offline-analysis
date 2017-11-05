include $(abs_top_srcdir)/Makefile.config

AM_CXXFLAGS = -Werror -Wall -Wno-unknown-pragmas

lib_LIBRARIES = libdft_api64.a

libdft_api64_a_SOURCES = dta_api64.cc libdft_api64.cc libdft_core64.cc syscall_desc64.cc tagmap64.cc debuglog.cc ins_seq_patterns.cc 
libdft_api64_a_CXXFLAGS = -DBIGARRAY_MULTIPLIER=1 -DUSING_XED -Wall -Werror -std=c++0x\
						   -Wno-unknown-pragmas -fno-stack-protector -DTARGET_IA32E \
						   -DHOST_IA32E -fPIC -DTARGET_LINUX -I$(PIN_ROOT)/source/include/pin \
						   -I$(PIN_ROOT)/source/include/pin/gen \
						   -I$(PIN_ROOT)/extras/components/include \
						   -I$(PIN_ROOT)/extras/xed2-intel64/include \
						   -I$(PIN_ROOT)/source/tool/InstLib -O0 -g -fomit-frame-pointer \
						   -fno-strict-aliasing

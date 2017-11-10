include $(abs_top_srcdir)/Makefile.config

AM_CXXFLAGS = -Werror -Wall -Wno-unknown-pragmas

lib_LTLIBRARIES = libdft64.la libdft-dta64.la

libdft64_la_SOURCES = libdft64.cc
libdft64_la_CXXFLAGS = -DBIGARRAY_MULTIPLIER=1 -DUSING_XED -Wall -Werror -std=c++0x\
						   -Wno-unknown-pragmas -fno-stack-protector -DTARGET_IA32E \
						   -DHOST_IA32E -fPIC -DTARGET_LINUX -I$(PIN_ROOT)/source/include/pin \
						   -I$(PIN_ROOT)/source/include/pin/gen \
						   -I$(PIN_ROOT)/extras/components/include \
						   -I$(PIN_ROOT)/extras/xed2-intel64/include \
						   -I$(PIN_ROOT)/source/tool/InstLib -O0 -g -fomit-frame-pointer \
						   -I$(srcdir)/../src \
						   -fno-strict-aliasing
libdft64_la_LDFLAGS = -Wl,--hash-style=sysv -Wl,-Bsymbolic \
						  -Wl,--version-script=$(PIN_ROOT)/source/include/pin/pintool.ver \
						  -L$(PIN_ROOT)/intel64/lib -L$(PIN_ROOT)/intel64/lib-ext \
						  -L$(PIN_ROOT)/intel64/runtime/glibc -L$(PIN_ROOT)/extras/xed2-intel64/lib \
						  -L$(builddir)/../src
libdft64_la_LIBADD = -ldft_api64 -lpin -lxed -ldwarf -lelf -ldl -lglib-2.0
libdft64_la_DEPENDENCIES = $(builddir)/../src/libdft_api64.a

libdft_dta64_la_SOURCES = libdft-dta64.cc
libdft_dta64_la_CXXFLAGS = -DBIGARRAY_MULTIPLIER=1 -DUSING_XED -Wall -Werror -std=c++0x\
						   -Wno-unknown-pragmas -fno-stack-protector -DTARGET_IA32E \
						   -DHOST_IA32E -fPIC -DTARGET_LINUX -I$(PIN_ROOT)/source/include/pin \
						   -I$(PIN_ROOT)/source/include/pin/gen \
						   -I$(PIN_ROOT)/extras/components/include \
						   -I$(PIN_ROOT)/extras/xed2-intel64/include \
						   -I$(PIN_ROOT)/source/tool/InstLib -O0 -g -fomit-frame-pointer \
						   -I$(srcdir)/../src \
               -I$(OMNIPLAY_DIR)/test \
               -I/usr/include/glib-2.0 \
               -I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
						   -fno-strict-aliasing
libdft_dta64_la_LDFLAGS = -Wl,--hash-style=sysv -Wl,-Bsymbolic \
						  -Wl,--version-script=$(PIN_ROOT)/source/include/pin/pintool.ver \
						  -L$(PIN_ROOT)/intel64/lib -L$(PIN_ROOT)/intel64/lib-ext \
						  -L$(PIN_ROOT)/intel64/runtime/glibc -L$(PIN_ROOT)/extras/xed2-intel64/lib \
						  -L$(builddir)/../src
libdft_dta64_la_LIBADD = -ldft_api64 -lpin -lxed -ldwarf -lelf -ldl 
libdft_dta64_la_DEPENDENCIES = $(builddir)/../src/libdft_api64.a

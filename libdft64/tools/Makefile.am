include $(abs_top_srcdir)/Makefile.config

AM_CXXFLAGS = -Werror -Wall -Wno-unknown-pragmas

lib_LTLIBRARIES = libsyscall64.la libdft64.la libdft-dta64.la

libsyscall64_la_SOURCES = syscall64.cc
libsyscall64_la_CXXFLAGS = -DBOOST_LOG_DYN_LINK -DBIGARRAY_MULTIPLIER=1 -DUSING_XED -DUSE_CUSTOM_TAG -DLIBDFT_TAG_TYPE=libdft_tag_set_uint32 -DTHEIA_REPLAY_COMPENSATION -Wall -Werror -std=c++0x\
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
libsyscall64_la_LDFLAGS = -Wl,--hash-style=sysv -Wl,-Bsymbolic \
						  -Wl,--version-script=$(PIN_ROOT)/source/include/pin/pintool.ver \
						  -L$(PIN_ROOT)/intel64/lib -L$(PIN_ROOT)/intel64/lib-ext \
						  -L$(PIN_ROOT)/intel64/runtime/glibc -L$(PIN_ROOT)/extras/xed2-intel64/lib \
						  -L$(OMNIPLAY_DIR)/test \
						  -L$(builddir)/../src
libsyscall64_la_LIBADD = -lpqxx -lpq -lutil -ldft_api64 -lpin -lxed -ldwarf -lelf -ldl
libsyscall64_la_DEPENDENCIES = $(builddir)/../src/libdft_api64.a

libdft64_la_SOURCES = libdft64.cc
libdft64_la_CXXFLAGS = -DBOOST_LOG_DYN_LINK -DBIGARRAY_MULTIPLIER=1 -DUSING_XED -Wall -Werror -std=c++0x\
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
						  -L$(OMNIPLAY_DIR)/test \
						  -L$(builddir)/../src
libdft64_la_LIBADD = -lpqxx -lpq -lutil -ldft_api64 -lpin -lxed -ldwarf -lelf -ldl -lglib-2.0
libdft64_la_DEPENDENCIES = $(builddir)/../src/libdft_api64.a

libdft_dta64_la_SOURCES = libdft-dta64.cc
libdft_dta64_la_CXXFLAGS = -DBOOST_LOG_DYN_LINK -DBIGARRAY_MULTIPLIER=1 -DUSING_XED -DUSE_CUSTOM_TAG -DLIBDFT_TAG_TYPE=libdft_tag_set_uint32 -DTHEIA_REPLAY_COMPENSATION -Wall -Werror -std=c++0x\
						   -Wno-unknown-pragmas -fno-stack-protector -DTARGET_IA32E \
						   -DHOST_IA32E -fPIC -DTARGET_LINUX -I$(PIN_ROOT)/source/include/pin \
						   -I$(PIN_ROOT)/source/include/pin/gen \
						   -I$(PIN_ROOT)/extras/components/include \
						   -I$(PIN_ROOT)/extras/xed2-intel64/include \
						   -I$(PIN_ROOT)/source/tool/InstLib -O0 -g -fomit-frame-pointer \
						   -I$(srcdir)/../src \
               -I$(OMNIPLAY_DIR)/test \
               -I$(OMNIPLAY_DIR)/analysis-agent/Theia_Tagging \
               -I$(OMNIPLAY_DIR)/query_generator \
               -I/usr/local/include/libtcta3cpp \
               -I/usr/include/glib-2.0 \
               -I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
						   -fno-strict-aliasing
libdft_dta64_la_LDFLAGS = -Wl,--hash-style=sysv -Wl,-Bsymbolic \
						  -Wl,--version-script=$(PIN_ROOT)/source/include/pin/pintool.ver \
						  -L$(PIN_ROOT)/intel64/lib -L$(PIN_ROOT)/intel64/lib-ext \
						  -L$(PIN_ROOT)/intel64/runtime/glibc -L$(PIN_ROOT)/extras/xed2-intel64/lib \
						  -L$(OMNIPLAY_DIR)/test \
						  -L$(builddir)/../src
libdft_dta64_la_LIBADD = -lutil -ldft_api64 -lpqxx -lpq -lpin -lxed -ldwarf -lelf -ldl -ltcta3cpp -lavrocpp -lrdkafka++ -lpthread -lboost_log -lboost_program_options -lboost_log_setup -lboost_thread -lboost_system -lboost_serialization -lboost_filesystem
libdft_dta64_la_DEPENDENCIES = $(builddir)/../src/libdft_api64.a

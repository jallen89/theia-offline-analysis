include $(abs_top_srcdir)/Makefile.config

syscall64 = #libsyscall64.la
dft64 = #libdft64.la
dft_dta64 = libdft_dta64.la
lib_LTLIBRARIES = $(syscall64) $(dft64) $(dft_dta64)

#libsyscall64_la_SOURCES = syscall64.cc
#libsyscall64_la_CXXFLAGS = $(COMMON_CXXFLAGS)
#libsyscall64_la_LDFLAGS = $(COMMON_LDFLAGS)
#libsyscall64_la_LIBADD = -lutil -ldft_api64 $(COMMON_LIBADD)
#libsyscall64_la_DEPENDENCIES = $(builddir)/../src/libdft_api64.a
#
#libdft64_la_SOURCES = libdft64.cc
#libdft64_la_CXXFLAGS = $(COMMON_CXXFLAGS)
#libdft64_la_LDFLAGS = $(COMMON_LDFLAGS)
#libdft64_la_LIBADD = -lutil -ldft_api64 $(COMMON_LIBADD)
#libdft64_la_DEPENDENCIES = $(builddir)/../src/libdft_api64.a

libdft_dta64_la_SOURCES = libdft-dta64.cc
libdft_dta64_la_CXXFLAGS = $(COMMON_CXXFLAGS) $(LIBDFT_DTA64_FLAGS)
libdft_dta64_la_LDFLAGS = $(COMMON_LDFLAGS) $(LIBDFT_DTA64_FLAGS)
libdft_dta64_la_LIBADD = -lutil -ldft_api64 $(COMMON_LIBADD)
libdft_dta64_la_DEPENDENCIES = $(builddir)/../src/libdft_api64.a

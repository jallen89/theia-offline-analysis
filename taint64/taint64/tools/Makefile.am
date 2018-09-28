include $(abs_top_srcdir)/Makefile.config

syscall64 = libsyscall64_u8.la libsyscall64_u16.la libsyscall64_u32.la libsyscall64_u64.la
dft64 = libdft64_u8.la libdft64_u16.la libdft64_u32.la libdft64_u64.la
dft_dta64 = libdft_dta64_u8.la libdft_dta64_u16.la libdft_dta64_u32.la libdft_dta64_u64.la
lib_LTLIBRARIES = $(syscall64) $(dft64) $(dft_dta64)

libsyscall64_u8_la_SOURCES = syscall64.cc
libsyscall64_u8_la_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE8_FLAGS)
libsyscall64_u8_la_LDFLAGS = $(COMMON_LDFLAGS)
libsyscall64_u8_la_LIBADD = -lutil -ldft_api64_u8 $(COMMON_LIBADD)
libsyscall64_u8_la_DEPENDENCIES = $(builddir)/../src/libdft_api64_u8.a

libsyscall64_u16_la_SOURCES = syscall64.cc
libsyscall64_u16_la_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE16_FLAGS)
libsyscall64_u16_la_LDFLAGS = $(COMMON_LDFLAGS)
libsyscall64_u16_la_LIBADD = -lutil -ldft_api64_u16 $(COMMON_LIBADD)
libsyscall64_u16_la_DEPENDENCIES = $(builddir)/../src/libdft_api64_u16.a

libsyscall64_u32_la_SOURCES = syscall64.cc
libsyscall64_u32_la_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE32_FLAGS)
libsyscall64_u32_la_LDFLAGS = $(COMMON_LDFLAGS)
libsyscall64_u32_la_LIBADD = -lutil -ldft_api64_u32 $(COMMON_LIBADD)
libsyscall64_u32_la_DEPENDENCIES = $(builddir)/../src/libdft_api64_u32.a

libsyscall64_u64_la_SOURCES = syscall64.cc
libsyscall64_u64_la_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE64_FLAGS)
libsyscall64_u64_la_LDFLAGS = $(COMMON_LDFLAGS)
libsyscall64_u64_la_LIBADD = -lutil -ldft_api64_u64 $(COMMON_LIBADD)
libsyscall64_u64_la_DEPENDENCIES = $(builddir)/../src/libdft_api64_u64.a

libdft64_u8_la_SOURCES = libdft64.cc
libdft64_u8_la_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE8_FLAGS)
libdft64_u8_la_LDFLAGS = $(COMMON_LDFLAGS)
libdft64_u8_la_LIBADD = -lutil -ldft_api64_u8 $(COMMON_LIBADD)
libdft64_u8_la_DEPENDENCIES = $(builddir)/../src/libdft_api64_u8.a

libdft64_u16_la_SOURCES = libdft64.cc
libdft64_u16_la_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE16_FLAGS)
libdft64_u16_la_LDFLAGS = $(COMMON_LDFLAGS)
libdft64_u16_la_LIBADD = -lutil -ldft_api64_u16 $(COMMON_LIBADD)
libdft64_u16_la_DEPENDENCIES = $(builddir)/../src/libdft_api64_u16.a

libdft64_u32_la_SOURCES = libdft64.cc
libdft64_u32_la_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE32_FLAGS)
libdft64_u32_la_LDFLAGS = $(COMMON_LDFLAGS)
libdft64_u32_la_LIBADD = -lutil -ldft_api64_u32 $(COMMON_LIBADD)
libdft64_u32_la_DEPENDENCIES = $(builddir)/../src/libdft_api64_u32.a

libdft64_u64_la_SOURCES = libdft64.cc
libdft64_u64_la_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE64_FLAGS)
libdft64_u64_la_LDFLAGS = $(COMMON_LDFLAGS)
libdft64_u64_la_LIBADD = -lutil -ldft_api64_u64 $(COMMON_LIBADD)
libdft64_u64_la_DEPENDENCIES = $(builddir)/../src/libdft_api64_u64.a

libdft_dta64_u8_la_SOURCES = libdft-dta64.cc
libdft_dta64_u8_la_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE8_FLAGS)
libdft_dta64_u8_la_LDFLAGS = $(COMMON_LDFLAGS)
libdft_dta64_u8_la_LIBADD = -lutil -ldft_api64_u8 $(COMMON_LIBADD)
libdft_dta64_u8_la_DEPENDENCIES = $(builddir)/../src/libdft_api64_u8.a

libdft_dta64_u16_la_SOURCES = libdft-dta64.cc
libdft_dta64_u16_la_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE16_FLAGS)
libdft_dta64_u16_la_LDFLAGS = $(COMMON_LDFLAGS)
libdft_dta64_u16_la_LIBADD = -lutil -ldft_api64_u16 $(COMMON_LIBADD)
libdft_dta64_u16_la_DEPENDENCIES = $(builddir)/../src/libdft_api64_u16.a

libdft_dta64_u32_la_SOURCES = libdft-dta64.cc
libdft_dta64_u32_la_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE32_FLAGS)
libdft_dta64_u32_la_LDFLAGS = $(COMMON_LDFLAGS)
libdft_dta64_u32_la_LIBADD = -lutil -ldft_api64_u32 $(COMMON_LIBADD)
libdft_dta64_u32_la_DEPENDENCIES = $(builddir)/../src/libdft_api64_u32.a

libdft_dta64_u64_la_SOURCES = libdft-dta64.cc
libdft_dta64_u64_la_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE64_FLAGS)
libdft_dta64_u64_la_LDFLAGS = $(COMMON_LDFLAGS)
libdft_dta64_u64_la_LIBADD = -lutil -ldft_api64_u64 $(COMMON_LIBADD)
libdft_dta64_u64_la_DEPENDENCIES = $(builddir)/../src/libdft_api64_u64.a

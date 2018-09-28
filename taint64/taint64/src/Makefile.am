include $(abs_top_srcdir)/Makefile.config

lib_LIBRARIES = libdft_api64_u8.a libdft_api64_u16.a libdft_api64_u32.a libdft_api64_u64.a

api64_sources = TheiaDB.cpp TheiaStoreCdmProducer.cpp dta_api64.cc libdft_api64.cc libdft_core64.cc syscall_desc64.cc tagmap64.cc debuglog.cc ins_seq_patterns.cc tag_traits.cc

libdft_api64_u8_a_SOURCES = $(api64_sources)
libdft_api64_u8_a_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE8_FLAGS)
libdft_api64_u16_a_SOURCES = $(api64_sources)
libdft_api64_u16_a_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE16_FLAGS)
libdft_api64_u32_a_SOURCES = $(api64_sources)
libdft_api64_u32_a_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE32_FLAGS)
libdft_api64_u64_a_SOURCES = $(api64_sources)
libdft_api64_u64_a_CXXFLAGS = $(COMMON_CXXFLAGS) $(SIZE64_FLAGS)

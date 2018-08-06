SUBDIRS = error

include $(abs_top_srcdir)/Makefile.config

$(srcdir)/package.m4: $(top_srcdir)/configure.ac
	:;{ \
	  echo "# Signature of the current package." && \
	  echo "m4_define([AT_PACKAGE_NAME],      [@PACKAGE_NAME@])" && \
	  echo "m4_define([AT_PACKAGE_TARNAME],   [@PACKAGE_TARNAME@])" && \
	  echo "m4_define([AT_PACKAGE_VERSION],   [@PACKAGE_VERSION@])" && \
	  echo "m4_define([AT_PACKAGE_STRING],    [@PACKAGE_STRING@])" && \
	  echo "m4_define([AT_PACKAGE_BUGREPORT], [@PACKAGE_BUGREPORT@])" && \
	  echo "m4_define([AT_PACKAGE_URL],       [@PACKAGE_URL@])"; \
	} > $@~
	mv $@~ $@

TESTSUITE_AT = $(srcdir)/testsuite.at

EXTRA_DIST = $(TESTSUITE_AT) $(srcdir)/package.m4 $(TESTSUITE) atlocal.in
TESTSUITE = $(srcdir)/testsuite

check-local: atconfig atlocal $(TESTSUITE) $(LINUX)
	$(SHELL) '$(TESTSUITE)' PIN='$(PIN_ROOT)/pin' $(TF)

installcheck-local: atconfig atlocal $(TESTSUITE)
	$(SHELL) '$(TESTSUITE)' PIN='$(PIN_ROOT)/pin' AUTOTEST_PATH='$(bindir)' $(TF)

clean-local:
	test ! -f '$(TESTSUITE)' || $(SHELL) '$(TESTSUITE)' --clean

AUTOM4TE = $(SHELL) $(top_srcdir)/build-aux/missing --run autom4te
AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): $(TESTSUITE_AT) $(srcdir)/package.m4
	$(AUTOTEST) -I '$(srcdir)' $@.at -o $@~
	mv $@~ $@

AM_CPPFLAGS = \
	-I$(top_builddir)/src/gtest/include \
	-I$(top_builddir)/src/mapi \
	-I$(top_builddir)/include

TESTS = glapi-test
check_PROGRAMS = glapi-test

glapi_test_SOURCES =			\
	check_table.cpp

glapi_test_LDADD = \
	$(top_builddir)/src/mapi/glapi/libglapi.a \
	$(top_builddir)/src/gtest/libgtest.la \
	-lpthread

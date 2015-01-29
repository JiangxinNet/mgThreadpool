lib_LTLIBRARIES=libmgThreadpool.la 
libmgThreadpool_la_CFLAGS=$(AM_FLGS) -fPIC -shared -D_LARGEFILE64_SOURCE=1 -D_GNU_SOURCE -lmgStd -lpthread
libmgThreadpool_la_LIBADD= 
libmgThreadpool_la_SOURCES=mgThreadpool.c
libmgThreadpool_HEADERS=mgThreadpool.h
libmgThreadpooldir=@includedir@/mgThreadpool

#bin_PROGRAMS=mglist_test

noinst_PROGRAMS=mgThreadpool_test

mgThreadpool_test_DEPENDENCIES=
mgThreadpool_test_CFLAGS= 
mgThreadpool_test_LDADD=#./libmgThreadpool.la 
mgThreadpool_test_SOURCES= mgThreadpool_test.c mgThreadpool.c
mgThreadpool_test_LDFLAGS= -lmgStd -lpthread



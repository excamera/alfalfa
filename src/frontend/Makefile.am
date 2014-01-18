AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../display -I$(srcdir)/../encoder $(X11_XCB_CFLAGS) $(GLU_CFLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)
LDADD = ../util/libalfalfautil.a ../encoder/libalfalfaencoder.a ../decoder/libalfalfadecoder.a ../display/libalfalfadisplay.a $(X11_XCB_LIBS) $(GLU_LIBS)
bin_PROGRAMS = vp8scan

vp8scan_SOURCES = vp8scan.cc

AM_CPPFLAGS = -I$(srcdir)/../decoder -I$(srcdir)/../display $(X11_XCB_CFLAGS) $(GLU_CFLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS)
LDADD = ../decoder/libalfalfadecoder.a ../display/libalfalfadisplay.a $(X11_XCB_LIBS) $(GLU_LIBS)
bin_PROGRAMS = vp8scan

vp8scan_SOURCES = vp8scan.cc

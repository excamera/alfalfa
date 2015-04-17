AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../display -I$(srcdir)/../encoder $(X11_XCB_CFLAGS) $(GLU_CFLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)
LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(X11_XCB_LIBS) $(GLU_LIBS)
bin_PROGRAMS = vp8decode vp8play diff-streams

vp8decode_SOURCES = vp8decode.cc

vp8play_SOURCES = vp8play.cc

diff_streams_SOURCES = diff-streams.cc

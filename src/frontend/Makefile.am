AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../display -I$(srcdir)/../encoder $(X11_XCB_CFLAGS) $(GLU_CFLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)
LDADD = ../util/libalfalfautil.a ../encoder/libalfalfaencoder.a ../decoder/libalfalfadecoder.a ../display/libalfalfadisplay.a $(X11_XCB_LIBS) $(GLU_LIBS)
bin_PROGRAMS = vp8decode vp8play diff-streams

vp8decode_SOURCES = vp8decode.cc

vp8play_SOURCES = vp8play.cc

diff_streams_SOURCES = diff-streams.cc

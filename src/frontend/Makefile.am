AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../display -I$(srcdir)/../encoder $(X11_XCB_CFLAGS) $(GLU_CFLAGS) $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)
LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(X11_XCB_LIBS) $(GLU_LIBS)
bin_PROGRAMS = vp8decode vp8play diff-sizes frameify frameplayer smartframeplayer

vp8decode_SOURCES = vp8decode.cc

vp8play_SOURCES = vp8play.cc

diff_sizes_SOURCES = diff-sizes.cc

frameify_SOURCES = frameify.cc

frameplayer_SOURCES = frameplayer.cc

smartframeplayer_SOURCES = smartframeplayer.cc

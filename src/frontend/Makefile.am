AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../display -I$(srcdir)/../encoder $(X11_XCB_CFLAGS) $(GLU_CFLAGS) $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)
bin_PROGRAMS = vp8decode vp8play diff-sizes frameify graphgen frameplayer collisions

vp8decode_SOURCES = vp8decode.cc
vp8decode_LDADD = -lx264 ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(X11_XCB_LIBS) $(GLU_LIBS)

vp8play_SOURCES = vp8play.cc
vp8play_LDADD = -lx264 ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(X11_XCB_LIBS) $(GLU_LIBS)

diff_sizes_SOURCES = diff-sizes.cc
diff_sizes_LDADD = -lx264 ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(X11_XCB_LIBS) $(GLU_LIBS)

frameify_SOURCES = frameify.cc
frameify_LDADD = -lx264 ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../util/libalfalfautil.a

graphgen_SOURCES = graphgen.cc
graphgen_LDADD = -lx264 ../decoder/libalfalfadecoder.a ../util/libalfalfautil.a

frameplayer_SOURCES = frameplayer.cc
frameplayer_LDADD = -lx264 ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(X11_XCB_LIBS) $(GLU_LIBS)

collisions_SOURCES = collisions.cc
collisions_LDADD = -lx264 ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(X11_XCB_LIBS) $(GLU_LIBS)

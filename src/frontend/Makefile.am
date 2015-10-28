AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../format -I$(srcdir)/../display -I$(srcdir)/../encoder $(GLU_CFLAGS) $(GLEW_CFLAGS) $(GLFW3_CFLAGS) $(X264_CFLAGS) $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

bin_PROGRAMS = vp8decode vp8play denseify frameify graphgen frameplayer collisions alfalfa-import

vp8decode_SOURCES = vp8decode.cc
vp8decode_LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(X264_LIBS)

vp8play_SOURCES = vp8play.cc
vp8play_LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(GLU_LIBS) $(X264_LIBS) $(GLEW_LIBS) $(GLFW3_LIBS)

frameify_SOURCES = frameify.cc
frameify_LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../util/libalfalfautil.a $(X264_LIBS)

denseify_SOURCES = denseify.cc
denseify_LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../util/libalfalfautil.a $(X264_LIBS)

graphgen_SOURCES = graphgen.cc
graphgen_LDADD = ../decoder/libalfalfadecoder.a ../util/libalfalfautil.a

frameplayer_SOURCES = frameplayer.cc
frameplayer_LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(GLU_LIBS) $(X264_LIBS) $(GLEW_LIBS) $(GLFW3_LIBS)

collisions_SOURCES = collisions.cc
collisions_LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(X264_LIBS)

alfalfa_import_SOURCES = alfalfa-import.cc
alfalfa_import_LDADD = ../format/libalfalfavideo.a ../format/protobufs/libalfalfaprotobufs.a ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../util/libalfalfautil.a $(X264_LIBS) $(PROTOBUF_LIBS)

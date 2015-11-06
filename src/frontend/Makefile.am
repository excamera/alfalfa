AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../format -I$(srcdir)/../display -I$(srcdir)/../encoder $(GLU_CFLAGS) $(GLEW_CFLAGS) $(GLFW3_CFLAGS) $(X264_CFLAGS) $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

bin_PROGRAMS = vp8decode vp8play collisions alfalfa-import alfalfa-combine alfalfa-describe

vp8decode_SOURCES = vp8decode.cc
vp8decode_LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(X264_LIBS)

vp8play_SOURCES = vp8play.cc
vp8play_LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(GLU_LIBS) $(X264_LIBS) $(GLEW_LIBS) $(GLFW3_LIBS)

collisions_SOURCES = collisions.cc
collisions_LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(X264_LIBS)

alfalfa_import_SOURCES = alfalfa-import.cc
alfalfa_import_LDADD = ../format/libalfalfavideo.a ../format/protobufs/libalfalfaprotobufs.a ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../util/libalfalfautil.a $(X264_LIBS) $(PROTOBUF_LIBS)

alfalfa_combine_SOURCES = alfalfa-combine.cc
alfalfa_combine_LDADD = ../format/libalfalfavideo.a ../format/protobufs/libalfalfaprotobufs.a ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../util/libalfalfautil.a $(X264_LIBS) $(PROTOBUF_LIBS)

alfalfa_describe_SOURCES = alfalfa-describe.cc
alfalfa_describe_LDADD = ../format/libalfalfavideo.a ../format/protobufs/libalfalfaprotobufs.a ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../util/libalfalfautil.a $(X264_LIBS) $(PROTOBUF_LIBS)

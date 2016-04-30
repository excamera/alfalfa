AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

noinst_LIBRARIES = libalfalfaencoder.a

libalfalfaencoder_a_SOURCES = frame_input.hh \
	ivf_reader.hh ivf_reader.cc \
	yuv4mpeg.hh yuv4mpeg.cc \
	dct.cc bool_encoder.hh serializer.cc encode_tree.cc continuation.cc \
	continuation_player.hh continuation_player.cc encoder.hh encoder.cc

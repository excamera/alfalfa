AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

noinst_LIBRARIES = libalfalfaencoder.a

libalfalfaencoder_a_SOURCES = frame_input.hh \
	ivf_reader.hh ivf_reader.cc \
	yuv4mpeg.hh yuv4mpeg.cc costs.hh costs.cc \
	bool_encoder.hh serializer.cc encode_tree.cc \
	encoder.hh encoder.cc \
	encode_intra.cc encode_inter.cc reencode.cc

AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../input $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

noinst_LIBRARIES = libalfalfaencoder.a

libalfalfaencoder_a_SOURCES =	variance.cc variance_sse2.cc \
	safe_references.cc costs.hh costs.cc \
	bool_encoder.hh serializer.cc encode_tree.cc \
	encoder.hh encoder.cc encode_intra.cc encode_inter.cc \
	reencode.cc size_estimation.cc

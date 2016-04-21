AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

noinst_LIBRARIES = libalfalfaencoder.a

libalfalfaencoder_a_SOURCES = dct.hh dct.cc bool_encoder.hh serializer.cc encode_tree.cc continuation.cc \
			      continuation_player.hh continuation_player.cc encoder.hh encoder.cc

AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I../format $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

noinst_LIBRARIES = libalfalfaencoder.a

libalfalfaencoder_a_SOURCES = bool_encoder.hh encoder.cc encode_tree.cc continuation.cc \
			      continuation_player.hh continuation_player.cc

AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../format -I../protobufs $(CXX11_FLAGS) $(PROTOBUF_CFLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

noinst_LIBRARIES = libalfalfaclient.a

libalfalfaclient_a_SOURCES = frame_fetcher.hh frame_fetcher.cc

AM_CPPFLAGS = -I$(srcdir)/../util $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS)

noinst_LIBRARIES = libnet.a

libnet_a_SOURCES = address.hh address.cc \
	socket.hh socket.cc \
	socketpair.hh socketpair.cc \
	packet.hh packet.cc \
	poller.hh poller.cc \
	pacer.hh

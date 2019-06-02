AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

noinst_LIBRARIES = libalfalfainput.a

libalfalfainput_a_SOURCES = frame_input.hh \
	ivf_reader.hh ivf_reader.cc \
	yuv4mpeg.hh yuv4mpeg.cc \
	camera.hh camera.cc \
	jpeg.hh jpeg.cc

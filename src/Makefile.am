AM_CPPFLAGS = -I$(srcdir)/../vp8
AM_CXXFLAGS = $(PICKY_CXXFLAGS)
LDADD = ../vp8/libdixie.a

frame_deps_LDADD = $(LDADD)

bin_PROGRAMS = frame_deps

frame_deps_SOURCES = frame_deps.cc frame_state.cc frame_state.hh operator_parser.cc operator_parser.hh test_vector_reader.cc test_vector_reader.hh

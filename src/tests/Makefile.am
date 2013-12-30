AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)
LDADD = ../util/libalfalfautil.a ../decoder/libalfalfadecoder.a $(X11_XCB_LIBS) $(GLU_LIBS)
check_PROGRAMS = extract-key-frames decode-to-stdout

extract_key_frames_SOURCES = extract-key-frames.cc
decode_to_stdout_SOURCES = decode-to-stdout.cc

dist_check_SCRIPTS = keyframe-decoding-test

TESTS = keyframe-decoding-test

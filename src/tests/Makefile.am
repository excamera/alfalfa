AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../encoder $(X264_CFLAGS) $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)
LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../util/libalfalfautil.a $(X264_LIBS)
check_PROGRAMS = extract-key-frames decode-to-stdout encode-loopback roundtrip switch-to-stdout \
		 frames-to-stdout single-frameify state-collisions

extract_key_frames_SOURCES = extract-key-frames.cc
decode_to_stdout_SOURCES = decode-to-stdout.cc
encode_loopback_SOURCES = encode-loopback.cc
roundtrip_SOURCES = roundtrip.cc
switch_to_stdout_SOURCES = switch-to-stdout.cc
frames_to_stdout_SOURCES = frames-to-stdout.cc
single_frameify_SOURCES = single-frameify.cc
state_collisions_SOURCES = state-collisions.cc

dist_check_SCRIPTS = decoding-test roundtrip-verify switch-test

TESTS = decoding-test encode-loopback roundtrip-verify switch-test

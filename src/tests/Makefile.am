AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../encoder $(X264_CFLAGS) $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)
LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../util/libalfalfautil.a $(X264_LIBS)
check_PROGRAMS = extract-key-frames decode-to-stdout encode-loopback roundtrip switch-to-stdout \
		 videodir-to-stdout state-collisions ivfcopy

extract_key_frames_SOURCES = extract-key-frames.cc
decode_to_stdout_SOURCES = decode-to-stdout.cc
encode_loopback_SOURCES = encode-loopback.cc
roundtrip_SOURCES = roundtrip.cc
switch_to_stdout_SOURCES = switch-to-stdout.cc
videodir_to_stdout_SOURCES = videodir-to-stdout.cc
state_collisions_SOURCES = state-collisions.cc
ivfcopy_SOURCES = ivfcopy.cc

dist_check_SCRIPTS = decoding-test roundtrip-verify switch-test

TESTS = decoding-test encode-loopback roundtrip-verify switch-test

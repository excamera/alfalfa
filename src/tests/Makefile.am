AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../encoder
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)
LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../util/libalfalfautil.a
check_PROGRAMS = extract-key-frames decode-to-stdout encode-loopback roundtrip

extract_key_frames_SOURCES = extract-key-frames.cc
decode_to_stdout_SOURCES = decode-to-stdout.cc
encode_loopback_SOURCES = encode-loopback.cc
roundtrip_SOURCES = roundtrip.cc

dist_check_SCRIPTS = decoding-test roundtrip-verify

TESTS = decoding-test encode-loopback roundtrip-verify

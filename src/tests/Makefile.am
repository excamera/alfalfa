AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../encoder $(X264_CFLAGS) $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../util/libalfalfautil.a $(X264_LIBS)

check_PROGRAMS = extract-key-frames decode-to-stdout encode-loopback roundtrip \
                 state-collisions ivfcopy ivfcompare

extract_key_frames_SOURCES = extract-key-frames.cc
decode_to_stdout_SOURCES = decode-to-stdout.cc
encode_loopback_SOURCES = encode-loopback.cc
roundtrip_SOURCES = roundtrip.cc
state_collisions_SOURCES = state-collisions.cc
ivfcopy_SOURCES = ivfcopy.cc
ivfcompare_SOURCES = ivfcompare.cc

dist_check_SCRIPTS = fetch-vectors.test fetch-encoder-vectors.test decoding.test \
                     roundtrip-verify.test switch-test ivfcopy.test \
                     xc-enc-ssim.test

TESTS = fetch-vectors.test decoding.test encode-loopback roundtrip-verify.test \
        ivfcopy.test fetch-encoder-vectors.test xc-enc-ssim.test

# some tests depend on the test vectors having been fetched
# represent the dependency in case of a parallel compile
decoding.log: fetch-vectors.log
roundtrip-verify.log: fetch-vectors.log
ivfcopy.log: fetch-vectors.log
xc-enc-ssim.log: fetch-encoder-vectors.log

clean-local:
	-rm -rf test_vectors
	-rm -rf encoder_test_vectors

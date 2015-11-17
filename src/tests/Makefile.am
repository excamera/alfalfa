AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../format -I$(srcdir)/../encoder -I../format $(X264_CFLAGS) $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)
LDADD = ../format/libalfalfavideo.a ../format/protobufs/libalfalfaprotobufs.a ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../util/libalfalfautil.a $(X264_LIBS) $(PROTOBUF_LIBS)
check_PROGRAMS = extract-key-frames decode-to-stdout encode-loopback roundtrip \
                 state-collisions ivfcopy framedb-test trackdb-test \
                 framedb-benchmark ivfcompare alfalfa-import-check alfalfa-combine-check alfalfa-encode-check alf-decode-to-stdout

extract_key_frames_SOURCES = extract-key-frames.cc
decode_to_stdout_SOURCES = decode-to-stdout.cc
alf_decode_to_stdout_SOURCES = alf-decode-to-stdout.cc
encode_loopback_SOURCES = encode-loopback.cc
roundtrip_SOURCES = roundtrip.cc
state_collisions_SOURCES = state-collisions.cc
ivfcopy_SOURCES = ivfcopy.cc
framedb_test_SOURCES = framedb-test.cc
trackdb_test_SOURCES = trackdb-test.cc
alfalfa_import_check_SOURCES = alfalfa-import-check.cc
alfalfa_combine_check_SOURCES = alfalfa-combine-check.cc
alfalfa_encode_check_SOURCES = alfalfa-encode-check.cc
framedb_benchmark_SOURCES = framedb-benchmark.cc
ivfcompare_SOURCES = ivfcompare.cc

dist_check_SCRIPTS = fetch-vectors.test decoding.test roundtrip-verify.test switch-test ivfcopy.test alfalfa-import.test alfalfa-combine.test alfalfa-encode.test alf-decoding.test alf-decoding-combine.test

TESTS = fetch-vectors.test decoding.test encode-loopback roundtrip-verify.test framedb-test ivfcopy.test trackdb-test alfalfa-import.test alfalfa-combine.test alfalfa-encode.test alf-decoding.test alf-decoding-combine.test # switch-test

# some tests depend on the test vectors having been fetched
# represent the dependency in case of a parallel compile
decoding.log: fetch-vectors.log
alf-decoding.log: fetch-vectors.log
roundtrip-verify.log: fetch-vectors.log
ivfcopy.log: fetch-vectors.log
alfalfa-import.log: fetch-vectors.log
alfalfa-combine.log: fetch-vectors.log
alfalfa-encode.log: fetch-vectors.log

clean-local:
	-rm -rf test_vectors

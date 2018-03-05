AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../display -I$(srcdir)/../input -I$(srcdir)/../encoder -I$(srcdir)/../net $(X264_CFLAGS) $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)
AM_LDFLAGS = $(STATIC_BUILD_FLAG)
BASE_LDADD = ../input/libalfalfainput.a ../decoder/libalfalfadecoder.a ../util/libalfalfautil.a $(X264_LIBS)

VP8PLAY_BUILD :=
if BUILDVP8PLAY
VP8PLAY_BUILD += vp8play
endif

bin_PROGRAMS = vp8decode xc-enc xc-ssim xc-dissect xc-framesize xc-dump \
               xc-diff comp-states xc-decode-bundle xc-merge \
               xc-terminate-chunk $(VP8PLAY_BUILD) \
               xc-zero-out-residues

vp8decode_SOURCES = vp8decode.cc
vp8decode_LDADD = ../encoder/libalfalfaencoder.a $(BASE_LDADD)

xc_decode_bundle_SOURCES = decode-bundle.cc
xc_decode_bundle_LDADD = ../encoder/libalfalfaencoder.a $(BASE_LDADD)

vp8play_SOURCES = vp8play.cc
vp8play_CPPFLAGS = $(AM_CPPFLAGS) $(GLU_CFLAGS) $(GLEW_CFLAGS) $(GLFW3_CFLAGS)
vp8play_LDADD = ../display/libalfalfadisplay.a $(BASE_LDADD) $(GLU_LIBS) $(GLEW_LIBS) $(GLFW3_LIBS)
# NOTE: Debian does not distribute static libs for libGLEW et al,
# so we have to override AM_LDFLAGS and always dynamically link
vp8play_LDFLAGS =

xc_enc_SOURCES = xc-enc.cc
xc_enc_LDADD = ../encoder/libalfalfaencoder.a $(BASE_LDADD)

xc_ssim_SOURCES = xc-ssim.cc
xc_ssim_LDADD = ../encoder/libalfalfaencoder.a $(BASE_LDADD)

xc_dissect_SOURCES = xc-dissect.cc
xc_dissect_LDADD = $(BASE_LDADD)

xc_framesize_SOURCES = xc-framesize.cc
xc_framesize_LDADD = $(BASE_LDADD)

xc_dump_SOURCES = xc-dump.cc
xc_dump_LDADD = $(BASE_LDADD)

xc_diff_SOURCES = xc-diff.cc
xc_diff_LDADD = $(BASE_LDADD)

xc_merge_SOURCES = xc-merge.cc
xc_merge_LDADD = $(BASE_LDADD)

xc_terminate_chunk_SOURCES = xc-terminate-chunk.cc
xc_terminate_chunk_LDADD = $(BASE_LDADD)

comp_states_SOURCES = comp-states.cc
comp_states_LDADD = $(BASE_LDADD)

xc_zero_out_residues_SOURCES = xc-zero-out-residues.cc
xc_zero_out_residues_LDADD = ../encoder/libalfalfaencoder.a $(BASE_LDADD)

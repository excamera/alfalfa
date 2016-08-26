AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../display -I$(srcdir)/../encoder $(GLU_CFLAGS) $(GLEW_CFLAGS) $(GLFW3_CFLAGS) $(X264_CFLAGS) $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

bin_PROGRAMS = vp8decode vp8play collisions xc-enc xc-ssim xc-dissect xc-framesize xc-stitch comp-states

vp8decode_SOURCES = vp8decode.cc
vp8decode_LDADD = ../decoder/libalfalfadecoder.a ../util/libalfalfautil.a $(X264_LIBS)

vp8play_SOURCES = vp8play.cc
vp8play_LDADD = ../decoder/libalfalfadecoder.a ../encoder/libalfalfaencoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(GLU_LIBS) $(X264_LIBS) $(GLEW_LIBS) $(GLFW3_LIBS)

collisions_SOURCES = collisions.cc
collisions_LDADD = ../decoder/libalfalfadecoder.a ../util/libalfalfautil.a $(X264_LIBS)

xc_enc_SOURCES = xc-enc.cc
xc_enc_LDADD = ../encoder/libalfalfaencoder.a ../decoder/libalfalfadecoder.a ../display/libalfalfadisplay.a ../util/libalfalfautil.a $(X264_LIBS) $(GLU_LIBS) $(GLEW_LIBS) $(GLFW3_LIBS)

xc_ssim_SOURCES = xc-ssim.cc
xc_ssim_LDADD = ../encoder/libalfalfaencoder.a ../decoder/libalfalfadecoder.a ../util/libalfalfautil.a $(X264_LIBS)

xc_dissect_SOURCES = xc-dissect.cc
xc_dissect_LDADD = ../encoder/libalfalfaencoder.a ../decoder/libalfalfadecoder.a ../util/libalfalfautil.a $(X264_LIBS)

xc_framesize_SOURCES = xc-framesize.cc
xc_framesize_LDADD = ../decoder/libalfalfadecoder.a ../util/libalfalfautil.a $(X264_LIBS)

xc_stitch_SOURCES = xc-stitch.cc
xc_stitch_LDADD = ../encoder/libalfalfaencoder.a ../decoder/libalfalfadecoder.a ../util/libalfalfautil.a $(X264_LIBS)

comp_states_SOURCES = comp-states.cc
comp_states_LDADD = ../encoder/libalfalfaencoder.a ../decoder/libalfalfadecoder.a ../util/libalfalfautil.a $(X264_LIBS)

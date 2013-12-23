AM_CPPFLAGS = $(X11_XCB_CFLAGS) $(GLU_CFLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS)
LDADD = $(X11_XCB_LIBS) $(GLU_LIBS)
bin_PROGRAMS = vp8scan

vp8scan_SOURCES = 2d.hh block.hh bool_decoder.cc bool_decoder.hh \
	chunk.hh exception.hh file.cc file_descriptor.hh file.hh \
	frame.cc frame_header.cc frame_header.hh frame.hh \
	ivf.cc ivf.hh macroblock_header.cc macroblock_header.hh \
	modemv_data.cc modemv_data.hh optional.hh prediction.cc \
	quantization.cc quantization.hh raster.hh \
	tokens.cc tokens.hh transform.cc tree.cc \
	uncompressed_chunk.cc uncompressed_chunk.hh \
	vp8_header_structures.hh vp8_parser.cc vp8_parser.hh \
	vp8_prob_data.cc vp8_prob_data.hh vp8scan.cc \
	display.hh display.cc

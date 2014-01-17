AM_CPPFLAGS = -I$(srcdir)/../util
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

noinst_LIBRARIES = libalfalfadecoder.a

libalfalfadecoder_a_SOURCES = block.hh bool_decoder.hh decoder.cc decoder.hh \
	frame.cc frame_header.hh frame.hh \
	loopfilter.cc loopfilter_filters.hh loopfilter.hh \
	macroblock.cc macroblock.hh modemv_data.cc modemv_data.hh \
	prediction.cc quantization.cc quantization.hh tokens.cc tokens.hh \
	transform.cc tree.cc uncompressed_chunk.cc uncompressed_chunk.hh \
	vp8_header_structures.hh vp8_prob_data.cc vp8_prob_data.hh scorer.hh

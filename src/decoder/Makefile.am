AM_CPPFLAGS = -I$(srcdir)/../util $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

SUFFIXES = .asm
.asm.o:
	$(AS) $(ASFLAGS) -I$(srcdir)/../asm/ $<

noinst_LIBRARIES = libalfalfadecoder.a

libalfalfadecoder_a_SOURCES = vp8_raster.hh block.hh bool_decoder.hh decoder.cc decoder.hh \
	safe_raster.cc \
	frame.cc frame_header.hh frame.hh \
	loopfilter.cc loopfilter_filters.hh loopfilter.hh \
	macroblock.cc macroblock.hh modemv_data.cc modemv_data.hh \
	prediction.cc quantization.cc quantization.hh tokens.cc tokens.hh \
	transform.cc tree.cc uncompressed_chunk.cc uncompressed_chunk.hh \
	vp8_header_structures.hh vp8_prob_data.cc vp8_prob_data.hh scorer.hh \
	decoder_state.hh loopfilter_sse2.asm loopfilter_block_sse2_x86_64.asm \
	predictor_sse.hh subpixel_ssse3.asm idctllm_mmx.asm \
	intrapred_ssse3.asm intrapred_sse2.asm intrapred_sse.hh \
	fwalsh_sse2.asm subtract_sse2.asm sad_sse2.asm sad_sse.hh \
	iwalsh_sse2.asm dct_sse2.asm dct_sse.hh \
	transform_sse.hh raster_handle.hh raster_handle.cc \
	player.cc player.hh probability_tables.cc enc_state_serializer.hh dct.cc \
	config.asm x86inc.asm x86_abi_support.asm \
	frame_pool.hh frame_pool.cc

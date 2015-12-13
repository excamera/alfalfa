AM_CPPFLAGS = -I$(srcdir)/../util -I../format $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

SUFFIXES = .asm
.asm.o:
	$(AS) $(ASFLAGS) -I$(srcdir)/../asm/ $<

noinst_LIBRARIES = libalfalfadecoder.a

libalfalfadecoder_a_SOURCES = vp8_raster.hh block.hh bool_decoder.hh decoder.cc decoder.hh \
	frame.cc frame_header.hh frame.hh \
	frame_info.cc frame_info.hh \
	loopfilter.cc loopfilter_filters.hh loopfilter.hh \
	macroblock.cc macroblock.hh modemv_data.cc modemv_data.hh \
	prediction.cc quantization.cc quantization.hh tokens.cc tokens.hh \
	transform.cc tree.cc uncompressed_chunk.cc uncompressed_chunk.hh \
	vp8_header_structures.hh vp8_prob_data.cc vp8_prob_data.hh scorer.hh \
	decoder_state.hh loopfilter_sse2.asm loopfilter_block_sse2_x86_64.asm \
	predictor_sse.hh subpixel_ssse3.asm idctllm_mmx.asm \
	transform_sse.hh raster_handle.hh raster_handle.cc \
	player.cc player.hh dependency_tracking.hh dependency_tracking.cc \
	tracking_player.hh tracking_player.cc probability_tables.cc \
	config.asm x86_abi_support.asm

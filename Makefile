source = frame_deps.cc  test_vector_reader.cc vp8_dixie_iface.cc dixie.cc modemv.cc predict.cc tokens.cc dixie_loopfilter.cc idct_add.cc vpx_decoder.cc vpx_image.cc vpx_mem.cc vpx_codec.cc bool_decoder.cc frame_state.cc operator_parser.cc
objects = test_vector_reader.o vp8_dixie_iface.o dixie.o modemv.o predict.o tokens.o dixie_loopfilter.o idct_add.o vpx_decoder.o vpx_image.o vpx_mem.o vpx_codec.o bool_decoder.o frame_state.o operator_parser.o
executables = frame_deps

CXX = g++
CXXFLAGS = -g -std=c++0x -ffast-math -pedantic -fno-default-inline -pipe -D_FILE_OFFSET_BITS=64 -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -fpermissive
LIBS = -lm -lrt

all: $(executables)

frame_deps: frame_deps.o $(objects)
	$(CXX) $(CXXFLAGS) -o $@ $+ $(LIBS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

-include depend

depend: $(source)
	$(CXX) $(INCLUDES) -MM $(source) > depend

.PHONY: clean
clean:
	-rm -f $(executables) depend *.o *.rpo

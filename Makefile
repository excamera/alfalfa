source = frame_deps.cc  test_vector_reader.cc vp8_dixie_iface.c dixie.c modemv.c predict.c tokens.c dixie_loopfilter.c idct_add.c vpx_decoder.c vpx_image.c vpx_mem.c vpx_codec.c bool_decoder.c frame_state.cc operator_parser.cc
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

%.o:%.c
	$(CXX)  $(CXXFLAGS) -c -o $@ $<

-include depend

depend: $(source)
	$(CXX) $(INCLUDES) -MM $(source) > depend

.PHONY: clean
clean:
	-rm -f $(executables) depend *.o *.rpo

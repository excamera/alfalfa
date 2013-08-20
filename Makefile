source = frame_deps.cc  test_vector_reader.cc vp8_dixie_iface.cc dixie.cc modemv.cc predict.cc tokens.cc dixie_loopfilter.cc idct_add.cc vpx_image.cc vpx_codec.cc frame_state.cc operator_parser.cc
objects = test_vector_reader.o vp8_dixie_iface.o dixie.o modemv.o predict.o tokens.o dixie_loopfilter.o idct_add.o vpx_image.o vpx_codec.o frame_state.o operator_parser.o
executables = frame_deps

CXX = g++
LANGFLAGS = --std=c++11
CXXFLAGS = -g -O3 $(LANGFLAGS) -ffast-math -pedantic -Wall -Wextra -Weffc++ -fno-default-inline -pipe -pthread -fpermissive
LIBS = -lm

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

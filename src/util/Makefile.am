AM_CPPFLAGS = $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

noinst_LIBRARIES = libalfalfautil.a

SUFFIXES = .asm
.asm.o:
	$(AS) $(ASFLAGS) -I$(srcdir)/../asm/ $<

libalfalfautil_a_SOURCES = 2d.hh chunk.hh exception.hh file.cc \
	file_descriptor.hh file.hh ivf.cc ivf.hh \
	optional.hh safe_array.hh raster.hh raster.cc ssim.hh ssim.cc \
	ivf_writer.hh ivf_writer.cc mmap_region.hh mmap_region.cc \
	finally.hh paranoid.hh paranoid.cc procinfo.hh procinfo.cc

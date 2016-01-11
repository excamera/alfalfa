all-local:
@BLANK@ifeq ($(srcdir),$(builddir))
	cd grpc && $(MAKE) $(AM_MAKEFLAGS) static_c static_cxx plugins
@BLANK@else
	cp -dR --no-preserve=mode --preserve=timestamps $(srcdir)/grpc . && cd grpc && $(MAKE) $(AM_MAKEFLAGS) static_c static_cxx plugins
@BLANK@endif

check-local:

clean-local:
@BLANK@ifeq ($(srcdir),$(builddir))
	cd grpc && $(MAKE) $(AM_MAKEFLAGS) clean
@BLANK@else
	rm -rf grpc
@BLANK@endif

install-data-local:

install-exec-local:

uninstall-local:

EXTRA_DIST = grpc

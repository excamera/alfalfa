all-local:
	cp -dR --no-preserve=mode --preserve=timestamps $(srcdir)/grpc.upstream ./grpc && cd grpc && $(MAKE) $(AM_MAKEFLAGS) static_c static_cxx plugins

check-local:

clean-local:
	rm -rf grpc

install-data-local:

install-exec-local:

uninstall-local:

EXTRA_DIST = grpc.upstream

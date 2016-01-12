source = alfalfa.proto

SUFFIXES = .proto .grpc.pb.cc .pb.cc

AM_CPPFLAGS = $(CXX11_FLAGS) $(PROTOBUF_CFLAGS)
PROTOC = ../../third_party/grpc/bins/opt/protobuf/protoc
PLUGIN = ../../third_party/grpc/bins/opt/grpc_cpp_plugin

.proto.pb.cc:
	$(AM_V_GEN)$(PROTOC) --cpp_out=. -I$(srcdir) $<

.proto.grpc.pb.cc:
	$(AM_V_GEN)$(PROTOC) --grpc_out=. -I$(srcdir) --plugin=protoc-gen-grpc=$(PLUGIN) $<

noinst_LIBRARIES = libalfalfaprotobufs.a

libalfalfaprotobufs_a_SOURCES = $(source)
nodist_libalfalfaprotobufs_a_SOURCES = $(source:.proto=.pb.cc) $(source:.proto=.pb.h) $(source:.proto=.grpc.pb.cc) $(source:.proto=.grpc.pb.h)

BUILT_SOURCES = $(source:.proto=.pb.cc) $(source:.proto=.grpc.pb.cc)
CLEANFILES = $(source:.proto=.pb.cc) $(source:.proto=.pb.h) $(source:.proto=.grpc.pb.cc) $(source:.proto=.grpc.pb.h)

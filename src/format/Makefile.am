AM_CPPFLAGS = -I../protobufs -I$(srcdir)/../util -I$(srcdir)/../decoder -I$(srcdir)/../client $(CXX11_FLAGS) $(PROTOBUF_CFLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

noinst_LIBRARIES = libalfalfavideo.a

libalfalfavideo_a_SOURCES = serialization.cc serialization.hh db.hh \
  manifests.cc  manifests.hh frame_db.cc frame_db.hh \
  alfalfa_video.cc alfalfa_video.hh \
  alfalfa_video_client.cc alfalfa_video_client.hh \
  alfalfa_video_service.cc alfalfa_video_service.hh \
  alfalfa_player.cc alfalfa_player.hh

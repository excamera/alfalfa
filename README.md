# Alfalfa [![Build Status](https://travis-ci.org/excamera/alfalfa.svg?branch=master)](https://travis-ci.org/excamera/alfalfa)

Alfalfa is a [VP8](https://en.wikipedia.org/wiki/VP8) encoder and decoder, implemented
in explicit state-passing style.

This implementation passes the VP8 conformance tests.

## Build directions

To build the source, you'll need the following packages:

* `g++` >= 5.0
* `yasm`
* `libxinerama-dev`
* `libxcursor-dev`
* `libglu1-mesa-dev`
* `libboost-all-dev`
* `libxrandr-dev`
* `libxi-dev`
* `libglew-dev`
* `libglfw3-dev`

The rest should be straightforward:

```
$ ./autogen.sh
$ ./configure
$ make -j$(nproc)
$ sudo make install
```


## Salsify

Source code for Salsify sender and reciever programs can be found at [`src/salsify`](https://github.com/excamera/alfalfa/tree/master/src/salsify).

First, run the receiver program:

```
salsify-receiver [PORT] 1280 720
```

Then, run the sender program:

```
salsify-sender --device [CAMERA, usually /dev/video0] [HOST] [PORT] 1337
```

The default pixel format is YUV420. Most webcams support raw YUV420, however the frame rate might be low.



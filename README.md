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
* `libx264-dev`
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
